#define _XOPEN_SOURCE 500
#define _ATFILE_SOURCE 

#include "cloudapi.h"
#include "cloudfs.h"
#include "dedup.h"

#define UNUSED __attribute__((unused))

static FILE *outfile;
static FILE *infile;

static char *logpath = "/tmp/cloudfs.log";
static FILE *logfd = NULL ;

static struct cloudfs_state state_;

void cloudfs_savetime(const char *fpath, struct timespec tv[2]){
	struct stat buf;
	lstat(fpath, &buf);

	tv[0].tv_sec = buf.st_atime;
	tv[1].tv_sec = buf.st_mtime;	
	tv[0].tv_nsec = 0;
	tv[1].tv_nsec = 0;	
} 

void cloudfs_generate_proxy(const char *fullpath, struct stat *buf){
	//int fd = creat(fullpath, buf->st_mode);
	truncate(fullpath, 0);
	
	int proxy = 1;
	lsetxattr(fullpath, "user.proxy", &proxy, sizeof(int), 0);
	lsetxattr(fullpath, "user.st_size", &(buf->st_size), sizeof(off_t), 0);	
	//lsetxattr(fullpath, "user.st_mode", &(buf->st_mode), sizeof(mode_t), 0);
	//lsetxattr(fullpath, "user.st_mtime", &(buf->st_mtime), sizeof(time_t), 0);
	//lsetxattr(fullpath, "user.st_blksize", &(buf->st_blksize), sizeof(blksize_t), 0);
}

int cloudfs_save_attribute(const char *fullpath, struct stat *buf){
	int ret = 0;
 	ret = lstat(fullpath, buf);
	if(ret < 0){
			log_msg("save attribute\n");
			return ret;
	}

	off_t size;
	ret = lgetxattr(fullpath, "user.st_size", &size, sizeof(off_t));
	if(ret > 0){
		log_msg("%s Save size attribute\n", fullpath);
		buf->st_size = size;
	}
	/*time_t time;
	ret = lgetxattr(fullpath, "user.st_mtime", &time, sizeof(time_t)); 
	if(ret > 0){
		log_msg("%s Save time attribute\n", fullpath);
		buf->st_mtime = time;
	}*/	
	return ret;
} 


int UNUSED cloudfs_change_attribute(const char *fullpath, const char *slavepath){
	int ret = 0;
	struct stat buf;
	ret = lstat(slavepath, &buf);
	
	if(ret < 0){
		//log_msg("LancerFS error: fail change attribute fullpath=%s, slavepath=%s\n", fullpath, slavepath);
		return ret;	
	}	
	lsetxattr(fullpath, "user.st_size", &(buf.st_size), sizeof(off_t), 0);
	//lsetxattr(fullpath, "user.st_mtime", &(buf.st_mtime), sizeof(time_t), 0);
	//lsetxattr(fullpath, "user.st_blksize", &(buf.st_blksize), sizeof(blksize_t), 0);

	return ret;
}

void cloudfd_set_attribute(const char *fullpath, struct stat *buf){
  lsetxattr(fullpath, "user.st_size", &(buf->st_size), sizeof(off_t), 0);
  //lsetxattr(fullpath, "user.st_mtime", &(buf->st_mtime), sizeof(time_t), 0);
  //lsetxattr(fullpath, "user.st_blksize", &(buf.st_blksize), sizeof(blksize_t));
	
}

int get_buffer(const char *buffer, int bufferLength) {
  return fwrite(buffer, 1, bufferLength, outfile);
}

int put_buffer(char *buffer, int bufferLength) {
  fprintf(logfd, "put_buffer %d \n", bufferLength);
  return fread(buffer, 1, bufferLength, infile);
}

void cloud_get_shadow(const char *fullpath, const char *cloudpath){
  //TODO: if need set mode
	outfile = fopen(fullpath, "wb");
  cloud_get_object("bkt", cloudpath, get_buffer);
	//*fd = outfile;
	fclose(outfile);
} 

void cloud_push_file(const char *fpath, struct stat *stat_buf){
	lstat(fpath, stat_buf);
	
  char cloudpath[MAX_PATH_LEN];
  memset(cloudpath, 0, MAX_PATH_LEN);
  strcpy(cloudpath, fpath);
  cloud_filename(cloudpath);
	
	infile = fopen(fpath, "rb");
	if(infile == NULL){
			log_msg("LancerFS error: cloud push %s failed\n", fpath);
			return;		
	}
	log_msg("LancerFS log: cloud_push_file(path=%s)\n", fpath);
  cloud_put_object("bkt", cloudpath, stat_buf->st_size, put_buffer);
  fclose(infile);	
}

void cloud_push_shadow(const char *fullpath, const char *shadowpath, struct stat *stat_buf){
	char cloudpath[MAX_PATH_LEN+3];
	memset(cloudpath, 0, MAX_PATH_LEN+3);
	strcpy(cloudpath, fullpath);
	
	infile = fopen(shadowpath, "rb");
  if(infile == NULL){
     log_msg("LancerFS error: cloud push %s failed, cloudpath %s, shadowpath %s\n", fullpath, cloudpath, shadowpath);
      return;
  }	

	
	cloud_filename(cloudpath);
 	log_msg("LancerFS log: cloud_push_file(path=%s)\n", cloudpath);
  lstat(shadowpath, stat_buf);
  cloud_put_object("bkt", cloudpath, stat_buf->st_size, put_buffer);
  fclose(infile);
}

void cloud_filename(char *path){
	while(*path != '\0'){
      if(*path == '/'){
          *path = '+';
      }
      path++;
  }
}

void cloud_slave_filename(char *path){
	sprintf(path, "%s%s", path, ".sl");	
}

void cloudfs_get_fullpath(const char *path, char *fullpath){
	sprintf(fullpath, "%s", state_.ssd_path);
	path++; 
	sprintf(fullpath, "%s%s", fullpath, path);	
}

void log_msg(const char *format, ...){
    va_list ap;
    va_start(ap, format);
    vfprintf(logfd, format, ap);
		fflush(logfd);
}

static int cloudfs_error(char *error_str)
{
    int retval = -errno;
   	log_msg(error_str); 
    return retval;
}

void cloudfs_log_close(){
	fclose(logfd);
}

void cloudfs_log_init(){
	logfd = fopen(logpath, "w");
	if(logfd == NULL){
		printf("LancerFS Error: connot find log file\n");
		exit(1);	
	}
}

int get_proxy(const char *fullpath){
  int proxy = 0;
  int r = lgetxattr(fullpath, "user.proxy", &proxy, sizeof(int));
	if(r < 0){
		proxy = 0;
	}
	return proxy;
}

int set_proxy(const char *fullpath, int proxy){
	return lsetxattr(fullpath, "user.proxy", &proxy, sizeof(int), 0);
}

int get_dirty(const char *fullpath){
	int dirty = 0;
	lgetxattr(fullpath, "user.dirty", &dirty, sizeof(int));
	return dirty;
}

int set_dirty(const char *fullpath, int dirty){
	return lsetxattr(fullpath, "user.dirty", &dirty, sizeof(int), 0); 
}

int set_slave(const char *fullpath, int slave){
	return lsetxattr(fullpath, "user.slave", &slave, sizeof(int), 0);
}

int get_slave(const char *fullpath){
	int slave;
	lgetxattr(fullpath, "user.slave", &slave, sizeof(int));
	return slave;
}

/*
 * Initializes the FUSE file system (cloudfs) by checking if the mount points
 * are valid, and if all is well, it mounts the file system ready for usage.
 *
 */
void *cloudfs_init(struct fuse_conn_info *conn UNUSED)
{
  cloud_init(state_.hostname);
  //cloud_list_service(list_service);
  //cloud_delete_bucket("bkt");
	int r = cloud_create_bucket("bkt");
  if(r != 0){
    exit(1);
  }
	return NULL;
}

void cloudfs_destroy(void *data UNUSED) {
  cloud_destroy();
}

int cloudfs_getattr(const char *path , struct stat *statbuf)
{
  int ret = 0;
	char fpath[MAX_PATH_LEN];
	cloudfs_get_fullpath(path, fpath);	

  log_msg("\ncfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
					path, statbuf);
	
	if(get_proxy(fpath)){
		ret = lstat(fpath, statbuf);
		log_msg("\ncfs_getattr_proxy(path=\"%s\"\n", fpath);	
	  lgetxattr(fpath, "user.st_size", &(statbuf->st_size), sizeof(off_t));
  	//lgetxattr(fpath, "user.st_mtime", &(statbuf->st_mtime), sizeof(time_t));
  	//lgetxattr(fpath, "user.st_blksize", &(statbuf->st_blksize), sizeof(blksize_t));
	
		if(ret != 0){
			ret = cloudfs_error("getattr lstat\n");
		}		
	}else{
			log_msg("\ncfs_getattr_local(path=\"%s\"\n", fpath); 
			ret = lstat(fpath, statbuf);	
			if(ret != 0){
				ret = cloudfs_error("getattr lstat\n");
			}
	}
  return ret;
}

int cloudfs_mkdir(const char *path, mode_t mode){
	int ret = 0;
  char fpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fpath);

    log_msg("\ncfd_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);
	ret = mkdir(fpath, mode);
	if(ret < 0){
			ret = cloudfs_error("mkdir fail\n");
	}
	return ret;
}

int cloudfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
	int ret = 0;
	DIR *dp;
	struct dirent *de;

  log_msg("\ncfs_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n", path, buf, filler, offset, fi);	
	dp = (DIR *)(uintptr_t) fi->fh;
	
	de = readdir(dp);
	if(de == 0){
		ret = cloudfs_error("readdir fail\n");
		return ret;	
	}

	do{
		if(strcmp(de->d_name, "lost+found") == 0){
			continue;
		}
		log_msg("calling filler with name %s\n", de->d_name);
		if(filler(buf, de->d_name, NULL, 0) != 0){
			return -ENOMEM;
		}
	}while((de = readdir(dp)) != NULL);
		
	return ret;	
}

int cloudfs_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int ret = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("\ncfs_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = %d)\n",
	    path, name, value, size);
    cloudfs_get_fullpath(path, fpath);
    
    ret = lgetxattr(fpath, name, value, size);
    if (ret < 0){
				ret = cloudfs_error("cfs_getxattr lgetxattr");
		}else{
				log_msg("    value = \"%s\"\n", value);
   	} 
    return ret;
}

int cloudfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags){
	int ret = 0;
	char fpath[MAX_PATH_LEN];	
  log_msg("\ncfs_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n", path, name, value, size, flags);
  cloudfs_get_fullpath(path, fpath);
   
  ret = lsetxattr(fpath, name, value, size, flags);
  if (ret < 0)
			ret = cloudfs_error("cfs_setxattr lsetxattr");
   
  return ret;
}


int cloudfs_access(const char *path, int mask)
{
  int ret = 0;
  char fpath[MAX_PATH_LEN];
   
  log_msg("\ncfs_access(path=\"%s\", mask=0%o)\n", path, mask);
  cloudfs_get_fullpath(path, fpath);
    
  ret = access(fpath, mask);
    
  if(ret < 0)
		ret = cloudfs_error("cfs_access access");
   return ret;
}

int cloudfs_mknod(const char *path, mode_t mode, dev_t dev){
	int ret = 0;
	char fpath[MAX_PATH_LEN];

  log_msg("\nmknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
	path, mode, dev);
	cloudfs_get_fullpath(path, fpath);

  if(S_ISREG(mode)){
     ret = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
		if(ret < 0)
	    ret = cloudfs_error("mknod open\n");
    else{
      ret = close(ret);
	  	if(ret < 0)
		  	 ret = cloudfs_error("mknod close\n");
	   }
  }else if(S_ISFIFO(mode)){
	    ret = mkfifo(fpath, mode);
	    if(ret < 0)
				ret = cloudfs_error("mknod mkfifo\n");
	}else{
	    ret = mknod(fpath, mode, dev);
	    if(ret < 0)
				ret = cloudfs_error("mknod mknod\n");
	}
 
	 return ret;	
}

int cloudfs_open(const char *path, struct fuse_file_info *fi){
	int ret = 0;
	int fd;
	char fpath[MAX_PATH_LEN];
		
  log_msg("\ncfd_open(path\"%s\", fi=0x%08x)\n", path, fi);
  cloudfs_get_fullpath(path, fpath);

  fd = open(fpath, fi->flags);
  if (fd < 0){
		ret = cloudfs_error("open fail\n");
		return ret;
	}
	//if has proxy flag, otherwise it is a new file
	int proxy;
	int r = 0;
	r = lgetxattr(fpath, "user.proxy", &proxy, sizeof(int));	 
 	if(r < 0){// this is a new file, set proxy as 0
		proxy = 0;
		lsetxattr(fpath, "user.proxy", &proxy, sizeof(int), 0); 
		log_msg("cfd_open, set proxy path=%s\n", path);
		
		//BUG
	/*	struct stat buf;
		lstat(fpath, &buf);
		int new = 1;
		lsetxattr(fpath, "user.st_mtime", &(buf.st_mtime), sizeof(time_t), 0);
		lsetxattr(fpath, "user.new", &new, sizeof(int), 0);*/	
	}else if(proxy == 0){//Small file that stored in SSD
			log_msg("open non proxy file %s\n", path);
	}else if(proxy == 1){// File opened is in cloud, only proxy file here
			log_msg("LancerFS log: open proxy file %s\n", path);
		  char cloudpath[MAX_PATH_LEN];
  		memset(cloudpath, 0, MAX_PATH_LEN);
  		strcpy(cloudpath, fpath);	
			cloud_filename(cloudpath);
				
			struct timespec tv[2];
			cloudfs_savetime(fpath, tv); 
			cloud_get_shadow(fpath, cloudpath);
			utimensat(0, fpath, tv, 0);		
			int dirty = 0;
					
			lsetxattr(fpath, "user.dirty", &dirty, sizeof(int), 0);
	}else{
		r = cloudfs_error("LFS error: wrong proxy file flag\n");  
	}
		
	fi->fh = fd;
  return ret;	
}

int cloudfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int ret = 0;
    log_msg("\ncfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);
    
    ret = pread(fi->fh, buf, size, offset);
    if (ret < 0)
				ret = cloudfs_error("cfs_read read");
    
    return ret;
}

int cloudfs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi){
	int ret = 0;
  log_msg("\ncfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);
	
  ret = pwrite(fi->fh, buf, size, offset);
  if(ret < 0){
		ret = cloudfs_error("pwrite fail\n");
   	return ret;
	}

  char fpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fpath);

	int r = 0;	
	//ret = lgetxattr(fpath, "user.slave", &slave, sizeof(int));
	if(get_proxy(fpath)){// set slave attribute before
			log_msg("dirty file %s\n", fpath);
			int dirty = 1;
			r = lsetxattr(fpath, "user.dirty", &dirty, sizeof(int), 0);
			if(r < 0){
				r = cloudfs_error("LancerFS eror: set dirty bit on failt\n");
				return r;	
			}
			//TODO: set time attribute
			//struct stat b;	
			//lstat(fpath, &b); 
			//lgetxattr(fpath, "user.st_time", &time, sizeof(time_t));
			//lsetxattr(fpath, "user.st_mtime", &(b.st_mtime), sizeof(time_t), 0); 
	}
  return ret;
}

int cloudfs_release(const char *path, struct fuse_file_info *fi){
	int ret = 0;
  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);	 
 	log_msg("\ncfs_release(path=\"%s\", fi=0x%08x)\n", path, fi);

	if(!get_proxy(fullpath)){
		//this is a local file
		log_msg("LancerFS log: release local file\n");
		struct stat buf;
		lstat(fullpath, &buf); 
		if(buf.st_size < state_.threshold){//small file, keep in SSD
			log_msg("LancerFS log: close local file\n");
			ret = close(fi->fh);		
			//goto done; 	
		}else{
			struct stat stat_buf;
			struct timespec tv[2];
      cloudfs_savetime(fullpath, tv);
			cloud_push_file(fullpath, &stat_buf);
		
			//delete current from SSD
			//assume only file can only be opened once
			//unlink(fullpath);

			//now file should be deleted
			//create a proxy file with same path
			cloudfs_generate_proxy(fullpath, &stat_buf);
			utimensat(0, fullpath, tv, 0);
		}
	}else{// a proxy file
			log_msg("LancerFS log: handle proxy file\n");	
			struct stat buf;							
		  struct timespec tv[2];
      cloudfs_savetime(fullpath, tv);	
			if(get_dirty(fullpath)){//dirty file, flush to Cloud
				cloud_push_shadow(fullpath, fullpath, &buf);		
			}
			
			//lstat(fullpath, &buf); 
			cloudfs_save_attribute(fullpath, &buf);
			//unlink(fullpath);
			truncate(fullpath, 0);
			cloudfs_generate_proxy(fullpath, &buf);
			utimensat(0, fullpath, tv, 0);	
			set_dirty(fullpath, 0);
	}
	
	return ret;
}

int cloudfs_opendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp;
    int ret = 0;
    char fpath[PATH_MAX];
    
    log_msg("\ncfs_opendir(path=\"%s\", fi=0x%08x)\n", path, fi);
    cloudfs_get_fullpath(path, fpath);
    
    dp = opendir(fpath);
    if (dp == NULL)
			ret = cloudfs_error("opendir fail\n");
    
    fi->fh = (intptr_t) dp;
    return ret;
}

int cloudfs_utimens(const char *path, const struct timespec tv[2]){
  int ret = 0;
  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);

	log_msg("Lancer log: cfs_utimens(path=%s)\n", fullpath);
	ret = utimensat(0, fullpath, tv, 0);
	//lsetxattr(fullpath, "user.st_mtime", &tv[1], sizeof(time_t), 0);	
	if(ret < 0){
		ret = cloudfs_error("utimes fail\n");
	}			
	return ret;	
}

int cloudfs_chmod(const char *path, mode_t mode)
{
  int ret = 0;
  char fpath[MAX_PATH_LEN];
    
  log_msg("\ncfs_chmod(fpath=\"%s\", mode=0%03o)\n", path, mode);
  cloudfs_get_fullpath(path, fpath); 
    
	ret = chmod(fpath, mode);
 	if(ret < 0)
		ret = cloudfs_error("chmod fail\n");
    
  return ret;
}

int cloudfs_unlink(const char *path)
{
    int retstat = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("cfs_unlink(path=\"%s\")\n",
	    path);
    cloudfs_get_fullpath(path, fpath);
   
		if(get_proxy(fpath)){
				char cloudpath[MAX_PATH_LEN];
				memset(cloudpath, 0, MAX_PATH_LEN);
				strcpy(cloudpath, fpath);
				cloud_filename(cloudpath);	
				cloud_delete_object("bkt", cloudpath);	
		}	
 
    retstat = unlink(fpath);
    if (retstat < 0)
				retstat = cloudfs_error("unlink fail");
    
    return retstat;
}

int cloudfs_rmdir(const char *path){
    int retstat = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("cfs_rmdir(path=\"%s\")\n", path);
    cloudfs_get_fullpath(path, fpath);
    
    retstat = rmdir(fpath);
    if (retstat < 0)
			retstat = cloudfs_error("cfs_rmdir rmdir");
    
    return retstat;
}

int cloudfs_truncate(const char *path, off_t newsize)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\ncloudfs_truncate(path=\"%s\", newsize=%lld)\n",
	    path, newsize);
    cloudfs_get_fullpath(path, fpath);
    
    retstat = truncate(fpath, newsize);
    if (retstat < 0)
			cloudfs_error("cloudfs_truncate truncate");
    
    return retstat;
}

/*
 * Functions supported by cloudfs 
 */
static struct fuse_operations cloudfs_operations = {
    .init           = cloudfs_init,
    .getattr        = cloudfs_getattr,
    .mkdir          = cloudfs_mkdir,
    .readdir        = cloudfs_readdir,
    .destroy        = cloudfs_destroy,
		.getxattr				= cloudfs_getxattr,
		.setxattr				= cloudfs_setxattr,
		.access					= cloudfs_access,
		.mknod					=	cloudfs_mknod,
		.open						= cloudfs_open,
		.read						= cloudfs_read,
		.write					= cloudfs_write,
		.release				= cloudfs_release,
		.opendir				= cloudfs_opendir,
		.utimens				= cloudfs_utimens,
		.chmod					= cloudfs_chmod,
		.unlink					= cloudfs_unlink,
		.rmdir					= cloudfs_rmdir,
		.truncate				= cloudfs_truncate
		
/*.readlink				=	readlink,
		.symlink				=	symlink,
		.rename					=	rename,
		.link						= link,
		.chown					=	chown,
		.listxattr			=	listxattr,
		.removexattr		=	removexattr,	
*/
};

int cloudfs_start(struct cloudfs_state *state,
                  const char* fuse_runtime_name) {

  int argc = 0;
  char* argv[10];
  argv[argc] = (char *) malloc(128 * sizeof(char));
  strcpy(argv[argc++], fuse_runtime_name);
  argv[argc] = (char *) malloc(1024 * sizeof(char));
  strcpy(argv[argc++], state->fuse_path);
  argv[argc++] = "-s"; // set the fuse mode to single thread
  //argv[argc++] = "-f"; // run fuse in foreground 

  state_  = *state;
	cloudfs_log_init();
  
	int fuse_stat = fuse_main(argc, argv, &cloudfs_operations, NULL);
  cloudfs_log_close(); 
  return fuse_stat;
}

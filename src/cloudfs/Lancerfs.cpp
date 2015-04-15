#include "Lancerfs.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "cloudapi.h"
#include "dedup.h"
#ifdef __cplusplus
}
#endif

#define UNUSED __attribute__((unused))


LancerFS::LancerFS(struct cloudfs_state *state){
	state_.init(state);
	
  logpath = "/tmp/cloudfs.log";
	//logpath = "/home/student/LancerFS/src/cloudfs.log";
  //init log
  logfd = fopen(logpath, "w");
  if(logfd == NULL){
    printf("LancerFS Error: connot find log file\n");
    //exit(1);
  }
	log_msg("LancerFS log: filesystem start\n");
  //init deduplication layer
	dup = new duplication(logfd, &state_);
}

void LancerFS::cloudfs_generate_proxy(const char *fullpath, struct stat *buf){
	int fd = creat(fullpath, buf->st_mode);
	log_msg("LancerFS log: create proxy file\n");
	if(fd < 0){
			log_msg("LancerFS error: fail to create proxy file %s\n", fullpath);	
	}
	int proxy = 1;
	lsetxattr(fullpath, "user.proxy", &proxy, sizeof(int), 0);
	lsetxattr(fullpath, "user.st_size", &(buf->st_size), sizeof(off_t), 0);	
	lsetxattr(fullpath, "user.st_mtime", &(buf->st_mtime), sizeof(time_t), 0);
	close(fd);
}

int LancerFS::cloudfs_save_attribute(const char *fullpath, struct stat *buf){
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
	time_t time;
	ret = lgetxattr(fullpath, "user.st_mtime", &time, sizeof(time_t)); 
	if(ret > 0){
		log_msg("%s Save time attribute\n", fullpath);
		buf->st_mtime = time;
	}	
	return ret;
} 

void LancerFS::cloudfs_set_attribute(const char *fullpath, struct stat *buf){
  lsetxattr(fullpath, "user.st_size", &(buf->st_size), sizeof(off_t), 0);
  lsetxattr(fullpath, "user.st_mtime", &(buf->st_mtime), sizeof(time_t), 0);
}

void LancerFS::cloud_filename(char *path){
	while(*path != '\0'){
      if(*path == '/'){
          *path = '+';
      }
      path++;
  }
}

void LancerFS::cloud_slave_filename(char *path){
	sprintf(path, "%s%s", path, ".sl");	
}

void LancerFS::cloudfs_get_fullpath(const char *path, char *fullpath){
	sprintf(fullpath, "%s", state_.ssd_path);
	path++; 
	sprintf(fullpath, "%s%s", fullpath, path);	
}

void LancerFS::log_msg(const char *format, ...){
    va_list ap;
    va_start(ap, format);
    vfprintf(logfd, format, ap);
		fflush(logfd);
}

void LancerFS::cloudfs_log_close(){
	fclose(logfd);
}

void LancerFS::cloudfs_log_init(){
	logfd = fopen(logpath, "w");
	if(logfd == NULL){
		printf("LancerFS Error: connot find log file\n");
		//exit(1);	
	}
}

int LancerFS::get_proxy(const char *fullpath){
  int proxy = 0;
  int r = lgetxattr(fullpath, "user.proxy", &proxy, sizeof(int));
	if(r < 0){
		proxy = 0;
	}
	return proxy;
}

int LancerFS::set_proxy(const char *fullpath, int proxy){
	return lsetxattr(fullpath, "user.proxy", &proxy, sizeof(int), 0);
}

int LancerFS::get_dirty(const char *fullpath){
	int dirty = 0;
	lgetxattr(fullpath, "user.dirty", &dirty, sizeof(int));
	return dirty;
}

int LancerFS::set_dirty(const char *fullpath, int dirty){
	return lsetxattr(fullpath, "user.dirty", &dirty, sizeof(int), 0); 
}

int LancerFS::set_slave(const char *fullpath, int slave){
	return lsetxattr(fullpath, "user.slave", &slave, sizeof(int), 0);
}

int LancerFS::get_slave(const char *fullpath){
	int slave;
	lgetxattr(fullpath, "user.slave", &slave, sizeof(int));
	return slave;
}

void *LancerFS::cloudfs_init(struct fuse_conn_info *conn UNUSED)
{
	return NULL;
}

void LancerFS::cloudfs_destroy(void *data UNUSED) {
  //cloud_destroy();
}


int LancerFS::cloudfs_getxattr(const char *path, const char *name,
                               char *value, size_t size)
{
    int ret = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("\ncfs_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, \
						size = %d)\n", path, name, value, size);
    cloudfs_get_fullpath(path, fpath);
    
    ret = lgetxattr(fpath, name, value, size);
    if (ret < 0){
				ret = cloudfs_error("cfs_getxattr lgetxattr");
		}else{
				log_msg("    value = \"%s\"\n", value);
   	} 
    return ret;
}

int LancerFS::cloudfs_setxattr(const char *path, const char *name,
                               const char *value, size_t size, int flags){
	int ret = 0;
	char fpath[MAX_PATH_LEN];	
  log_msg("\ncfs_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", \
            size=%d, flags=0x%08x)\n", path, name, value, size, flags);
  cloudfs_get_fullpath(path, fpath);
   
  ret = lsetxattr(fpath, name, value, size, flags);
  if (ret < 0)
			ret = cloudfs_error("cfs_setxattr lsetxattr");
   
  return ret;
}


int LancerFS::cloudfs_access(const char *path, int mask)
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

int LancerFS::cloudfs_mknod(const char *path, mode_t mode, dev_t dev){
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

int LancerFS::cloudfs_open(const char *path, struct fuse_file_info *fi){
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
	}else if(proxy == 0){//Small file that stored in SSD
			log_msg("open non proxy file %s\n", path);
	}else if(proxy == 1){// File opened is in cloud, only proxy file here
			log_msg("LancerFS log: open proxy file %s\n", path);
			int size = dup->get_file_size(fpath);
		
			if(size >= state_.ssd_size){
				string s(fpath);	
				superfiles.insert(s);		
			}else{	
				dup->retrieve(fpath);
			}	
			//int slave = 1;
			int dirty = 0;
			lsetxattr(fpath, "user.dirty", &dirty, sizeof(int), 0);
	}else{
		r = cloudfs_error("LFS error: wrong proxy file flag\n");  
	}
		
	fi->fh = fd;
  return ret;	
}

int LancerFS::cloudfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int ret = 0;
    log_msg("\ncfs_superfiles.insert(s);read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, \
                fi=0x%08x)\n", path, buf, size, offset, fi);
  
	  char fpath[MAX_PATH_LEN];
  	cloudfs_get_fullpath(path, fpath); 
		set<string>::iterator iter;	
		string s(fpath);
		iter = superfiles.find(s);
		if(iter != superfiles.end()){
			ret = dup->offset_read(fpath, buf, size, offset);	
		}else{
    	ret = pread(fi->fh, buf, size, offset);
    	if(ret < 0)
				ret = cloudfs_error("cfs_read read");
		} 
    return ret;
}

int LancerFS::cloudfs_write(const char *path, const char *buf, size_t size,
                            off_t offset, struct fuse_file_info *fi)
{
	int ret = 0;
  log_msg("\ncfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, \
          fi=0x%08x)\n", path, buf, size, offset, fi);
	
  ret = pwrite(fi->fh, buf, size, offset);
  if(ret < 0){
		ret = cloudfs_error("pwrite fail\n");
   	return ret;
	}

  char fpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fpath);

	int r = 0;	
	if(get_proxy(fpath)){
			log_msg("dirty file %s\n", fpath);
			int dirty = 1;
			r = lsetxattr(fpath, "user.dirty", &dirty, sizeof(int), 0);
			if(r < 0){
				r = cloudfs_error("LancerFS eror: set dirty bit on failt\n");
				return r;	
			}
	}
  return ret;
}

int LancerFS::cloudfs_release(const char *path, struct fuse_file_info *fi){
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
		}else{
			struct stat stat_buf;
			lstat(fullpath, &stat_buf);
			//cloud_push_file(fullpath, &stat_buf);
			dup->deduplicate(fullpath);
			unlink(fullpath);	
			cloudfs_generate_proxy(fullpath, &stat_buf);
		}
	}else{// a proxy file
			log_msg("LancerFS log: handle proxy file\n");	
		
			set<string>::iterator iter;	
			string s(fullpath);
			iter = superfiles.find(s);
			if(iter != superfiles.end()){
					superfiles.erase(iter);
			}	
			
			if(get_dirty(fullpath)){//dirty file, flush to Cloud
				dup->clean(fullpath);	
			}
		
			//time bug?
			/*
			*	if dirty, timestamp of file is correct file, but here doesn't 
			*	save that one.
			*/	
			struct stat buf;	
			cloudfs_save_attribute(fullpath, &buf);
			unlink(fullpath);
			cloudfs_generate_proxy(fullpath, &buf);	
			set_dirty(fullpath, 0);
	}
	return ret;
}

int LancerFS::cloudfs_opendir(const char *path, struct fuse_file_info *fi)
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

int LancerFS::cloudfs_utimens(const char *path, const struct timespec tv[2]){
  int ret = 0;
  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);

	log_msg("Lancer log: cfs_utimens(path=%s)\n", fullpath);
	ret = utimensat(0, fullpath, tv, 0);
	lsetxattr(fullpath, "user.st_mtime", &tv[1], sizeof(time_t), 0);	
	if(ret < 0){
		ret = cloudfs_error("utimes fail\n");
	}			
	return ret;	
}

int LancerFS::cloudfs_chmod(const char *path, mode_t mode)
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

int LancerFS::cloudfs_unlink(const char *path)
{
    int retstat = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("cfs_unlink(path=\"%s\")\n",
	    path);
    cloudfs_get_fullpath(path, fpath);
   
		if(get_proxy(fpath)){
				dup->remove(fpath);		
		}	
 
    retstat = unlink(fpath);
    if (retstat < 0)
				retstat = cloudfs_error("unlink fail");
    
    return retstat;
}

int LancerFS::cloudfs_rmdir(const char *path){
    int retstat = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("cfs_rmdir(path=\"%s\")\n", path);
    cloudfs_get_fullpath(path, fpath);
    
    retstat = rmdir(fpath);
    if (retstat < 0)
			retstat = cloudfs_error("cfs_rmdir rmdir");
    
    return retstat;
}

int LancerFS::cloudfs_truncate(const char *path, off_t newsize)
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

int LancerFS::cloudfs_error(char *error_str)
{
    int retval = -errno;
   	log_msg(error_str); 
    return retval;
}

LancerFS::LancerFS(){
	logpath = "/tmp/cloudfs.log";

	//init log
	logfd = fopen(logpath, "w");
	if(logfd == NULL){
		printf("LancerFS Error: connot find log file\n");
		//exit(1);	
	}
	log_msg("LancerFS log: filesystem start\n");
	//init deduplication layer
	dup = new duplication(logfd, state_.ssd_path);	
}

LancerFS::~LancerFS(){
	fclose(logfd);
	
	//memory management
	delete dup;	
}

int LancerFS::cloudfs_getattr(const char *path, struct stat *statbuf){
  int ret = 0;
	char fpath[MAX_PATH_LEN];
	cloudfs_get_fullpath(path, fpath);	

  log_msg("\ncfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
					path, statbuf);
	
	if(get_proxy(fpath)){
		ret = lstat(fpath, statbuf);
		log_msg("\ncfs_getattr_proxy(path=\"%s\"\n", fpath);
	  lgetxattr(fpath, "user.st_size", &(statbuf->st_size), sizeof(off_t));
		//TODO::how to handle time
		lgetxattr(fpath, "user.st_mtime", &(statbuf->st_mtime), sizeof(time_t));
	
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

int LancerFS::cloudfs_readdir(const char *path, void *buf, 
                            fuse_fill_dir_t filler, off_t offset,
                            struct fuse_file_info *fi)
{
	int ret = 0;
	DIR *dp;
	struct dirent *de;

  log_msg("\ncfs_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x,  \
					offset=%lld, fi=0x%08x)\n", path, buf, filler, offset, fi);	
	dp = (DIR *)(uintptr_t) fi->fh;
	
	de = readdir(dp);
	if(de == 0){
		cloudfs_error("readdir fail\n");
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

int LancerFS::cloudfs_mkdir(const char *path, mode_t mode){
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


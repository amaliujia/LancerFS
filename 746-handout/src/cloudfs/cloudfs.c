#include "cloudapi.h"
#include "cloudfs.h"
#include "dedup.h"
#include "cloudService.h"

//#define UNUSED __attribute__((unused))
#define DEBUG
static char *logpath = "/home/student/LancerFS/746-handout/src/log/trace.log";

static struct cloudfs_state _state;

/*************
	Tool Box
**************/

void cloudfs_get_fullpath(const char *path, char *fullpath){
	sprintf(fullpath, "%s", _state.ssd_path);
	path++; 
	sprintf(fullpath, "%s%s", fullpath, path);	
}

void cloudfs_generate_proxy(const char *fullpath, struct stat *buf){
	int fd = creat(fullpath, O_RDWR | O_CREAT);
	if(fd < 0){
			fprintf(logfd, "LancerFS Error: fail to create proxy file %s\n", 
							fullpath);	
			fflush(logfd);	
	}
	int proxy = 1;
	lsetxattr(fullpath, "user.proxy", &proxy, sizeof(int), 0);
	lsetxattr(fullpath, "user.st_size", &(buf->st_size), sizeof(off_t), 0);	
	lsetxattr(fullpath, "user.st_mode", &(buf->st_mode), sizeof(mode_t), 0);
} 

int cloudfs_error(char *error_str)
{
    int retval = -errno;
    fprintf(logfd, "LancerFS Error: %s\n", error_str);
		fflush(logfd);	
    /* FUSE always returns -errno to caller (yes, it is negative errno!) */
    return retval;
}

int cloudfs_debug(char *err){
	int ret = 0;
	#ifdef DEBUG
	fprintf(logfd, "LancerFS Debug: %s\n", err);
	fflush(logfd);
	ret = -errno;	
	#endif
	return ret;
}

void cloufds_log_close(){
	fclose(logfd);
}

void cloudfs_log_init(){
	logfd = fopen(logpath, "w");
	if(logfd == NULL){
		// shutdown
		printf("LancerFS Error: connot find log file\n");
		exit(1);	
	}
	print_cloudfs_state();
}

void print_cloudfs_state(){
	fprintf(logfd, "Fuse path: %s\n", _state.fuse_path);
	fprintf(logfd, "SSD path: %s\n", _state.ssd_path);
	fflush(logfd);
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
	int r = lgetxattr(fullpath, "user.dirty", &dirty, sizeof(int));
	return dirty;
}

int set_dirty(const char *fullpath, int dirty){
	return lsetxattr(fullpath, "user.dirty", &dirty, sizeof(int), 0); 
}

int set_slave(const char *fullpath, int slave){
	return lsetxattr(fullpath, "user.path", &slave, sizeof(int), 0);
}

/*
 * Initializes the FUSE file system (cloudfs) by checking if the mount points
 * are valid, and if all is well, it mounts the file system ready for usage.
 *
 */
void *cloudfs_init(struct fuse_conn_info *conn)
{
  cloud_init(_state.hostname);
  return NULL;
}

void cloudfs_destroy(void *data) {
	cloud_destroy();
}

int cloudfs_getattr(const char *path, struct stat *statbuf)
{
  int ret = 0;
  char fullpath[MAX_PATH_LEN] = {0};
  cloudfs_get_fullpath(path, fullpath);	
 
	fprintf(logfd, "LancerFS log: cfs_getattr(path=%s)\n", fullpath);
 
	ret = lstat(fullpath, statbuf);
	if(ret < 0){
		 //char errMsg[MAX_MSG_LEN];
     //fprintf(errMsg, "LancerFS error: cfs_getattr(path=%s)\n", fullpath); 
     //ret = cloudfs_error(errMsg);
		fprintf(logfd, "LancerFS error: cfs_getattr(path=%s), lstat=%d\n", fullpath, ret);
		ret = -errno;
	}
	fflush(logfd);
	return ret;
}

int cloudfs_mkdir(const char *path, mode_t mode){
	int ret = 0;
		
	char fullpath[MAX_PATH_LEN] = {0};
	cloudfs_get_fullpath(path, fullpath);

	fprintf(logfd, "LancerFS log: cfs_mkdir(path=%s)\n", fullpath);
	fflush(logfd);	
	
	ret = mkdir(fullpath, mode);
	if(ret < 0){
			fprintf(logfd, "mkdir returns error code %d", errno);
			fflush(logfd);
			ret = -errno;	
	}
	return ret;	
}

int cloudfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
												off_t offset, struct fuse_file_info *fileInfo)
{
	int ret = 0;
	struct dirent *dirent = NULL;
	DIR *dirp = NULL;	

  //fprintf(logfd, "LancerFS log: cloudfs_readdir(path=%s)\n", path);
  //fflush(logfd);

	dirp = (DIR *)(uintptr_t)fileInfo->fh;	
	dirent = readdir(dirp);		
	if(dirent == NULL){
			return -errno;
	}
	
	do{
		//fprintf(logfd, "LancerFS log: readdir: filler with name=%s)\n", dirent->d_name);
	  //fflush(logfd);
		if(filler(buf, dirent->d_name, NULL, 0) != 0){
				return -ENOMEM;
		}	
	}while((dirent = readdir(dirp)) != NULL);	
		
	return ret;
}

int cloudfs_open(const char *path, struct fuse_file_info *fileInfo){
	int ret = 0;
	int fd = -1;
	char fullpath[MAX_PATH_LEN];

	cloudfs_get_fullpath(path, fullpath);
  
	fprintf(logfd, "LancerFS log: cloudfs_open(path=%s)\n", fullpath);
  fflush(logfd);
							
	fd = open(fullpath, fileInfo->flags);
//	fd = open(fullpath, O_RDWR | O_CREAT, S_IRUSR|S_IWUSR);	
	if(fd < 0){
			char errMsg[MAX_MSG_LEN];
			sprintf(errMsg, "cfs_open(fullpath %s)\n", fullpath);
			ret = cloudfs_error(errMsg);
			
			// open file with fullpath fail, goto error handle part	
			goto done;
	}

	//if has proxy flag, otherwise it is a new file
	int proxy = -1;
	int r = lgetxattr(fullpath, "user.proxy", &proxy, sizeof(int));	
	if(r < 0){// this is a new file, set proxy as 0
		proxy = 0;
		lsetxattr(fullpath, "user.proxy", &proxy, sizeof(int), 0);	
		fprintf(logfd, "LancerFS log: new file, set proxy to file %s\n", 
						fullpath);
		fflush(logfd);
	}else if(proxy == 0){//Small file that stored in SSD
			fprintf(logfd, "LancerFS log: open non proxy file %s\n", fullpath);
			fflush(logfd);
	}else if(proxy == 1){// File opened is in cloud, only proxy file here
			fprintf(logfd, "LancerFS log: open proxy file %s\n", fullpath);
		  char cloudpath[MAX_PATH_LEN];
  		memset(cloudpath, 0, MAX_PATH_LEN);
  		strcpy(cloudpath, fullpath);	
			cloud_filename(cloudpath);
				
			char slavepath[MAX_PATH_LEN+3];
 			memset(slavepath, 0, MAX_PATH_LEN + 3);
  	  strcpy(slavepath, fullpath);
			cloud_slave_filename(slavepath);	
    	fprintf(logfd, "LancerFS log: create slave file %s by cloud file %s\n",
            slavepath, cloudpath);
			fflush(logfd);
			cloud_get_shadow(slavepath, cloudpath, &fd);
			int slave = 1;
			int dirty = 0;
					
			lsetxattr(fullpath, "user.slave", &slave, sizeof(int), 0);	
			lsetxattr(fullpath, "user.dirty", &dirty, sizeof(int), 0);	
	}else{
		// unknow proxy flag
		fprintf(logfd, "LancerFS error: wrong proxy flag %d\n", proxy);	
		goto error;	
	}	
	
//fileInfo->fh = fd;

done:
	fileInfo->fh = fd;
	return ret;	
error:
	fprintf(logfd, "LancerFS error: cannot open file %s\n", fullpath);
	fflush(logfd);
	ret = -errno;
	goto done;	
}

int cloudfs_read(const char *path, char *buf, size_t size, off_t offset, 
																					struct fuse_file_info *fileInfo)
{
	int ret = 0;

  char fullpath[MAX_PATH_LEN] = {0};
  cloudfs_get_fullpath(path, fullpath);

  fprintf(logfd, "LancerFS log: cloudfs_read(path=%s)\n", fullpath);
  fflush(logfd);

	ret = pread(fileInfo->fh, buf, size, offset);	
	if(ret < 0){
      char errMsg[MAX_MSG_LEN];
      sprintf(errMsg, "cannot read file %s\n", fullpath); 
      ret = cloudfs_error(errMsg);
	}
	return ret;
}

int cloudfs_rmdir(const char *path){
  int ret = 0;

  char fullpath[MAX_PATH_LEN] = {0};
  cloudfs_get_fullpath(path, fullpath);

  fprintf(logfd, "LancerFS log: cloudfs_rmdir(path=%s)\n", fullpath);	
	fflush(logfd);	
  
	ret = rmdir(fullpath);
  if(ret < 0){
      char errMsg[MAX_MSG_LEN];
      sprintf(errMsg, "cannot remove path %s\n", fullpath);
      ret = cloudfs_error(errMsg); 
	}
	return ret;	
}

int cloudfs_write(const char *path, const char *buf, size_t size, off_t offset, 
																			struct fuse_file_info *fileInfo)
{
	int ret = 0;
  char fullpath[MAX_PATH_LEN] = {0};
  cloudfs_get_fullpath(path, fullpath);

	//fprintf(logfd, "LancerFS debug: cfs_write(path=%s)\n", fullpath);
	
	fprintf(logfd, "LancerFS log: cloudfs_write(path=%s, size=%zu,  \
						\n", fullpath, size);
	fflush(logfd);
	
	lseek(fileInfo->fh, offset, SEEK_SET);
	ret = write(fileInfo->fh, buf, size);
	//ret = pwrite(fileInfo->fh, buf, size, offset);	
	if(ret < 0){
     char errMsg[MAX_MSG_LEN];
     sprintf(errMsg, "cannot write file %s (id=%d)with offset %zu size %d at  \
							buf 0x%08x\n", fullpath, fileInfo->fh, offset, size, buf);
     ret = cloudfs_error(errMsg);
		 goto done;
	}
		
	int slave = -1;
	ret = lgetxattr(fullpath, "user.slave", &slave, sizeof(int));
	if(ret > 0){// set slave attribute before
			if(slave == 0){
				fprintf(logfd, "LancerFS error: wrong slave id for proxy file %s\n",
								fullpath);
				goto error;
			}
			int dirty = 1;
			ret = lsetxattr(fullpath, "user.dirty", &dirty, sizeof(int), 0);
			if(ret < 0){
				fprintf(logfd, "LancerFS eror: set dirty bit on %s failt\n",
								fullpath);
				goto error;	
			}	
	}

done:
	return ret;
error:
	fprintf(logfd, "Unknown error in cloudfs_write\n");
	fflush(logfd);
	ret = -errno;
	goto done;	
}

int cloudfs_chmod(const char *path, mode_t mode){
	int ret = 0;
  char fullpath[MAX_PATH_LEN] = {0};
  cloudfs_get_fullpath(path, fullpath);	

	fprintf(logfd, "LancerFS log: cfs_chmod(path=%s)\n", fullpath);
	fflush(logfd);
	
	ret = chmod(fullpath, mode);
	if(ret < 0){
      char errMsg[MAX_MSG_LEN];
      sprintf(errMsg, "cannot chmod file %s by mode 0x%08x\n",
              fullpath, mode);
      ret = cloudfs_error(errMsg);
	}
	
	return ret;
}

int cloudfs_opendir(const char *path, struct fuse_file_info *fileInfo){
	int ret = 0;
	char fullpath[MAX_PATH_LEN] = {0};
  cloudfs_get_fullpath(path, fullpath);

	//fprintf(logfd, "LancerFS log: cfs_opendir(path=%s)\n", fullpath);
	//fflush(logfd);
	DIR *dirp = NULL;
	dirp = opendir(fullpath);	
	
	if(dirp == NULL){
      char errMsg[MAX_MSG_LEN];
      sprintf(errMsg, "cannot open dir %s\n", fullpath);
      ret = cloudfs_error(errMsg);
	}
	fileInfo->fh = (intptr_t)dirp;	
	
	return ret;

}


int cloudfs_getxattr(const char *path, const char *name, char *value, size_t size){
	int ret = 0;
  char fullpath[MAX_PATH_LEN] = {0};
  cloudfs_get_fullpath(path, fullpath);

  fprintf(logfd, "LancerFS log: cloudfs_getxattr(path=%s)\n", fullpath);
  fflush(logfd);

	ret = getxattr(fullpath, name, value, size);
	if(ret < 0){
      char errMsg[MAX_MSG_LEN];
      sprintf(errMsg, "cannot getxattr of %s\n", fullpath);
      ret = cloudfs_error(errMsg);
	}	
	return ret;
}

int cloudfs_setxattr(const char *path, const char *name, const char *value, 
																										size_t size, int flags)
{
	int ret = 0;
  char fullpath[MAX_PATH_LEN] = {0};
  cloudfs_get_fullpath(path, fullpath);

  fprintf(logfd, "LancerFS log: cloudfs_setxattr(path=%s)\n", fullpath);
  fflush(logfd);

	ret = setxattr(fullpath, name, value, size, flags);
	if(ret < 0){
      char errMsg[MAX_MSG_LEN];
      sprintf(errMsg, "cannot setxattr of %s\n", fullpath);
      ret = cloudfs_error(errMsg);
	}
	return ret;
} 

int cloudfs_unlink(const char *path){
	int ret = 0;
  char fullpath[MAX_PATH_LEN] = {0};
  cloudfs_get_fullpath(path, fullpath);

  fprintf(logfd, "LancerFS log: cloudfs_unlink(path=%s)\n", fullpath);
  fflush(logfd);

	ret = unlink(fullpath);
	if(ret < 0){
      char errMsg[MAX_MSG_LEN];
      sprintf(errMsg, "cannot unlink %s\n", fullpath);
      ret = cloudfs_error(errMsg);
	}
	return ret;
}

int cloudfs_release(const char *path, struct fuse_file_info *fileInfo){
	int ret = 0;
  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);	

  fprintf(logfd, "LancerFS log: cloudfs_release(path=%s)\n", fullpath);
  fflush(logfd);

	if(!get_proxy(fullpath)){
		//this is a local file
		fprintf(logfd, "LancerFS log:release local file\n", fullpath);
		struct stat buf;
		lstat(fullpath, &buf); 
		if(buf.st_size < _state.threshold){//small file, keep in SSD
			fprintf(logfd, "LancerFS log: close local file\n", fullpath);
			ret = close(fileInfo->fh);		
			goto done; 	
		}
		//push this file to cloud
		/*char cloudpath[MAX_PATH_LEN];
		memset(cloudpath, 0, MAX_PATH_LEN);
		strcpy(cloudpath, fullpath);	
		cloud_filename(cloudpath);	
		struct stat stat_buf;
		cloud_push_file(cloudpath, &stat_buf);

		//delete current from SSD
		//assume only file can only be opened once
		cloudfs_unlink(path);			
	
		//now file should be deleted
		//create a proxy file with same path
		cloudfs_generate_proxy(fullpath, &stat_buf);*/
	}else{// a proxy file
			fprintf(logfd, "LancerFS log: handle proxy file\n", fullpath);	
			struct stat buf;							
      char slavepath[MAX_PATH_LEN+3];
      memset(slavepath, 0, MAX_PATH_LEN + 3);
      strcpy(slavepath, fullpath);
      cloud_slave_filename(slavepath);
		
		if(get_dirty(fullpath)){//dirty file, flush to Cloud
			cloud_push_shadow(fullpath, slavepath, &buf);		
		}
		
		//delete slave file	
		close(fileInfo->fh);
		//unlink(slavepath);	
		//set proxy file attribute?
		set_dirty(fullpath, 0);
		set_slave(fullpath, 0); 
	}
	
done:	
	fflush(logfd);
	return ret;
} 

int cloudfs_utimens(const char *path, const struct timespec ts[2]){
  int ret = 0;
  char fullpath[MAX_PATH_LEN] = {0};
  cloudfs_get_fullpath(path, fullpath);

	fprintf(logfd, "Lancer log: cfs_utimens(path=%s)\n", fullpath);
	ret = utimensat(0, fullpath, ts, 0);
	if(ret < 0){
			fprintf(logfd, "Lancer error: cfs_utimens(path=%s)\n", fullpath);
			ret = -errno;	
	}			
	fflush(logfd);	
	return ret;	
}


int cloudfs_mknod(const char *path, mode_t mode, dev_t dev){
	int ret = 0;
  char fullpath[MAX_PATH_LEN] = {0};
  cloudfs_get_fullpath(path, fullpath);

	printf(logfd, "Lancer log: cfs_mknod(path=%s, mode=0x%08x)\n", 
					fullpath, mode);
	if(S_ISREG(mode)){
			ret = open(fullpath, O_CREAT | O_EXCL | O_WRONLY, mode);
			if(ret < 0){
				fprintf(logfd, "Lancer error: cfs_mknod, create(path=%s) faile\n", 
								fullpath);	
				goto error;
			}
			ret = close(ret);
			if(ret < 0){
        fprintf(logfd, "Lancer error: cfs_mknod, close new (path=%s) faile\n", 
                fullpath);
        goto error;
			}	
		
	}else if(S_ISFIFO(mode)){
			ret = mkfifo(fullpath, mode);
			if(ret < 0){
        fprintf(logfd, "Lancer error: cfs_mknod, fifo(path=%s) faile\n",
                fullpath);
				goto error;
			}		
	}else{
			ret = mknod(fullpath, mode, dev);
			if(ret < 0){
        fprintf(logfd, "Lancer error: cfs_mknod, mknod (path=%s) faile\n",
                fullpath);
				goto error; 
			}
	}

done:
	fflush(logfd);
	return ret;
error:
	ret = -errno;
	goto done;
}

int cloudfs_access(const char *path, int mode){
	int ret = 0;
  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);

	//fprintf(logfd, "Lancer log: cfs_access(path=%s, mode=0x%08x)\n",
   //       fullpath, mode);	
	ret = access(fullpath, mode);
	if(ret < 0){ 
     fprintf(logfd, "Lancer error: cfs_access, mknod(path=%s) faile\n",
		         fullpath);
			ret = -errno;	
	}

	fflush(logfd);
	return ret;
}

int cloudfs_create(const char *path, mode_t mode, struct fuse_file_info *fileInfo){
	int ret = 0;
	int fd;
  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);
	
  fprintf(logfd, "Lancer log: cfs_create(path=%s, mode=0x%08x)\n",
         fullpath, mode); 
	fflush(logfd);
		
	fd = creat(fullpath, mode);
	if(fd < 0){
		//ret = cloudfs_error("cloudfs fail to create\n");
     fprintf(logfd, "Lancer error: cfs create file fail\n");
			ret = -errno;
	}
	fileInfo->fh = fd;	
	return ret;
}

int cloudfs_truncate(const char *path, off_t newSize){
	int ret = 0;
  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);	
	fprintf(logfd, "Lancer log: cloudfs_truncate(path=%s)\n", path);
	fflush(logfd);

	ret = truncate(fullpath, newSize);
	if(ret < 0){
		//ret = cloudfs_error("cloudfs fail to truncate\n");
	}		
	
	return ret;
}

/*
 * Functions supported by cloudfs 
 */
// --- http://fuse.sourceforge.net/doxygen/structfuse__operations.html
static 
struct fuse_operations cloudfs_operations = {
    .init           = cloudfs_init,
    .getattr        = cloudfs_getattr,
    .mkdir          = cloudfs_mkdir,
    .readdir        = cloudfs_readdir,
    .open						= cloudfs_open,
		.destroy        = cloudfs_destroy,
		.rmdir					= cloudfs_rmdir,
		.write					= cloudfs_write,
		.read						= cloudfs_read,
		.chmod					= cloudfs_chmod,
		.opendir				= cloudfs_opendir,
		.getxattr				= cloudfs_getxattr,
		.setxattr				= cloudfs_setxattr,
		.unlink					= cloudfs_unlink,
		.release				= cloudfs_release,
		.utimens				= cloudfs_utimens,
		//.utime					= cloudfs_utime,
		.mknod					= cloudfs_mknod,
		.access					= cloudfs_access,
		.truncate				= cloudfs_truncate,
		.create					= cloudfs_create 
		//.utime					= cloudfs_utime
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

  _state  = *state;
	cloudfs_log_init();
	cloud_create_bucket("bkt");	
	int fuse_stat = fuse_main(argc, argv, &cloudfs_operations, NULL);

	cloufds_log_close();    
	return fuse_stat;
}

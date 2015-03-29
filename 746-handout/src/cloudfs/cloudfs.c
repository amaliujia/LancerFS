#include "cloudapi.h"
#include "cloudfs.h"
#include "dedup.h"

//#define UNUSED __attribute__((unused))
#define DEBUG

static char *logpath = "/home/student/LancerFS/746-handout/src/log/trace.log";
static FILE *logfd = NULL;

static struct cloudfs_state _state;

/*************
	Tool Box
**************/
void cloudfs_get_fullpath(const char *path, char *fullpath){ 
	sprintf(fullpath, "%s%s", _state.ssd_path, path);	
}

static int cloudfs_error(char *error_str)
{
    int retval = -errno;
    fprintf(logfd, "LancerFS Error: %s\n", error_str);
	
    /* FUSE always returns -errno to caller (yes, it is negative errno!) */
    return retval;
}

static int cloudfs_debug(char *err){
	int ret = 0;
	#ifdef DEBUG
	fprintf(logfd, "LancerFS Debug: %s\n", err);
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
			fprintf(logfd, "LancerFS Error: connot find log file\n");
		exit(1);	
	}
	print_cloudfs_state();
}

void print_cloudfs_state(){
	fprintf(logfd, "Fuse path: %s\n", _state.fuse_path);
	fprintf(logfd, "SSD path: %s\n", _state.ssd_path);
}

int get_proxy(const char *path){
  int proxy = 0;
  int r = lgetxattr(path, "Lancer.proxy", &proxy, sizeof(int));
	return proxy;
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
  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);	
  
	//local file?
	ret = lstat(fullpath, statbuf);
	if(ret < 0){
		fprintf(logfd, "LancerFS error: cfs_getattr(path=%s)\n", fullpath);
	}
	return ret;
}

int cloudfs_mkdir(const char *path, mode_t mode){
	int ret = 0;
		
	char fullpath[MAX_PATH_LEN];
	cloudfs_get_fullpath(path, fullpath);
	
	ret = mkdir(fullpath, mode);

	if(ret != 0){
			printf("mkdir returns error code %d", errno);	
	}

	return ret;	
}

int cloudfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
												off_t offset, struct fuse_file_info *fileInfo)
{
	int ret = 0;
	struct dirent *dirent = NULL;
	DIR *dirp = NULL;	

  char debugMsg[MAX_MSG_LEN];
  sprintf(debugMsg, "cfs_readdir(fullpath %s, buf=0x%08x, filler=0x%08x,	\
	offset=%d, fileInfo=0x%08x\n", path, (unsigned int)buf, filler, offset, 
	(unsigned int)fileInfo);
  cloudfs_debug(debugMsg);


	dirp = (DIR *)(uintptr_t)fileInfo->fh;	
	dirent = readdir(dirp);		
	if(dirent == NULL){
			return -errno;
	}
	
	do{
	  sprintf(debugMsg, "filler with name %s\n", dirent->d_name); 
  	cloudfs_debug(debugMsg);
		if(filler(buf, dirent->d_name, NULL, 0) != 0){
				return -ENOMEM;
		}	
	}while((dirent = readdir(dirp)) != NULL);	
	
	return ret;
}

int cloudfs_open(const char *path, struct fuse_file_info *fileInfo){
	int ret = 0;
	int fd;
	char fullpath[MAX_PATH_LEN];

	cloudfs_get_fullpath(path, fullpath);

	char debugMsg[MAX_MSG_LEN];
	sprintf(debugMsg, "cfs_open(fullpath %s, fi = 0x%08x)\n", 
					fullpath, fileInfo->flags);
	cloudfs_debug(debugMsg);
								
	fd = open(fullpath, fileInfo->flags);
	if(fd < 0){
			char errMsg[MAX_MSG_LEN];
			// open file with fullpath fail, goto error handle part	
			goto error;
	}

	//if has proxy flag, or if it is a new file
	int proxy = 0;
	int r = lgetxattr(fullpath, "Lancer.proxy", &proxy, sizeof(int));	
	if(r < 0){// this is a new file, set proxy as 0
		proxy = 0;
		lsetxattr(fullpath, "Lancer.proxy", &proxy, sizeof(int), 0);	
		fprintf(logfd, "LancerFS log: create and set proxy to file %s\n", 
						fullpath);	
	}else if(proxy == 0){//Small file that stored in SSD
			fprintf(logfd, "LancerFS log: open non proxy file %s\n", fullpath);
	}else if(proxy == 1){// File opened is in cloud, only proxy file here
			//sprintf(logfd, "LancerFS log: open non proxy file %s\n", fullpath);
	}else{
		// unknow proxy flag
		fprintf(logfd, "LancerFS error: wrong proxy flag %d\n", proxy);	
		goto error;	
	}	
	
	fileInfo->fh = fd;

done:
	return ret;	
error:
	sprintf(logfd, "LancerFS error: cannot open file %s\n", fullpath);
	goto done;	
}

int cloudfs_read(const char *path, char *buf, size_t size, off_t offset, 
																					struct fuse_file_info *fileInfo)
{
	int ret = 0;

  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);

	char debugMsg[MAX_MSG_LEN];
	sprintf(debugMsg, "cfs_read(fullpath %s, buf = 0x%08x, size = %d, \ 
					offset = %d, fi = 0x%08x)\n", fullpath, (int)buf, size,
					offset, (int)fileInfo);
	cloudfs_debug(debugMsg);

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

  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);

  char debugMsg[MAX_MSG_LEN];
  sprintf(debugMsg, "cfs_rmdir(fullpath %s)\n", fullpath);
  cloudfs_debug(debugMsg);
	
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
  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);

  char debugMsg[MAX_MSG_LEN];
  sprintf(debugMsg, "cfs_write(fullpath %s, buf = 0x%08x, size = %zu, \ 
          offset = %d, fi = 0x%08x)\n", fullpath, (unsigned int)buf, size,
          offset, (unsigned int)fileInfo);
  cloudfs_debug(debugMsg);
	
	if(!get_proxy(fullpath)){//local file, write data
		fprintf(logfd, "LancerFS log: cloudfs_write(path=%s, buf=%d, size=%d,  \
						offset=%d, fileInfo=%d\n", fullpath, buf, size, offset, fileInfo);
		//lseek(fileInfo->fh, offset, SEEK_SET);
		//ret = write(fileInfo->fh, buf, size);
	  ret = pwrite(fileInfo->fh, buf, size, offset);	
		if(ret < 0){
      char errMsg[MAX_MSG_LEN];
      sprintf(errMsg, "cannot write file %s with offset %zu size %d at  \
							buf 0x%08x\n", fullpath, offset, size, buf);
      ret = cloudfs_error(errMsg);
		}
		struct stat buf;
		ret = cloudfs_getattr(path, &buf);
		if(ret < 0){
    	fprintf(logfd, "LancerFS error: cannot read file stat in write\n");  
		}else if(buf.st_size >= _state.threshold){//to big, flush to Cloud
			
		}	
		goto done; // regular finish	
	}else{//Cloud file

	}

done:	
	return ret;	
}

int cloudfs_chmod(const char *path, mode_t mode){
	int ret = 0;
  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);	

  char debugMsg[MAX_MSG_LEN];
  sprintf(debugMsg, "cfs_chmod(fullpath %s, mode = 0x%08x)\n",
          fullpath, mode);
  cloudfs_debug(debugMsg);

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
	char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);

	char debugMsg[MAX_MSG_LEN];
  sprintf(debugMsg, "cfs_opendir(fullpath %s, fileInfo=0x%08x)\n",
          fullpath, fileInfo);
  cloudfs_debug(debugMsg);

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
  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);

  char debugMsg[MAX_MSG_LEN];
  sprintf(debugMsg, "cfs_getxattr(fullpath %s, name=%s, value=%s, size=%zu)\n",
          fullpath, name, value, size);
  cloudfs_debug(debugMsg);	

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
  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);

  char debugMsg[MAX_MSG_LEN];
  sprintf(debugMsg, "cfs_setxattr(fullpath %s, name=%s, value=%s, size=%zu, \ 
					flag=%d)\n", fullpath, name, value, size, flags);
  cloudfs_debug(debugMsg);

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
  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);

  char debugMsg[MAX_MSG_LEN];
  sprintf(debugMsg, "cfs_unlink(path=%s)\n", fullpath);
  cloudfs_debug(debugMsg);

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


	return ret;
} 

/*
 * Functions supported by cloudfs 
 */
static 
struct fuse_operations cloudfs_operations = {
    .init           = cloudfs_init,
    // --- http://fuse.sourceforge.net/doxygen/structfuse__operations.html
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
		.unlink					= cloudfs_unlink
		//TODO: release, utimens, mknod
		//.release				= cloudfs_release
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
	
	int fuse_stat = fuse_main(argc, argv, &cloudfs_operations, NULL);

	cloufds_log_close();    
	return fuse_stat;
}

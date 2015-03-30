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
	sprintf(fullpath, "%s%s", _state.ssd_path, path);	
}

void cloudfs_generate_proxy(const char *fullpath, struct stat *buf){
	int fd = creat(fullpath, O_RDWR | O_CREAT);
	if(fd < 0){
			fprintf(logfd, "LancerFS Error: fail to create proxy file %s\n", 
							fullpath);	
	}
	int proxy = 1;
	lsetxattr(fullpath, "user.proxy", &proxy, sizeof(int), 0);
	lsetxattr(fullpath, "user.st_size", &(buf->st_size), sizeof(off_t), 0);	
	lsetxattr(fullpath, "user.st_mode", &(buf->st_mode), sizeof(mode_t), 0);
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
	
int get_proxy(const char *fullpath){
  int proxy = 0;
  int r = lgetxattr(fullpath, "user.proxy", &proxy, sizeof(int));
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
								
	fd = open(fullpath, fileInfo->flags, O_CREAT);
	if(fd < 0){
			char errMsg[MAX_MSG_LEN];
			sprintf(errMsg, "cfs_open(fullpath %s)\n", fullpath);
			ret = cloudfs_error(errMsg);
			// open file with fullpath fail, goto error handle part	
			goto error;
	}

	//if has proxy flag, otherwise it is a new file
	int proxy = -1;
	int r = lgetxattr(fullpath, "user.proxy", &proxy, sizeof(int));	
	if(r < 0){// this is a new file, set proxy as 0
		proxy = 0;
		lsetxattr(fullpath, "user.proxy", &proxy, sizeof(int), 0);	
		fprintf(logfd, "LancerFS log: create and set proxy to file %s\n", 
						fullpath);	
	}else if(proxy == 0){//Small file that stored in SSD
			fprintf(logfd, "LancerFS log: open non proxy file %s\n", fullpath);
	}else if(proxy == 1){// File opened is in cloud, only proxy file here
			//sprintf(logfd, "LancerFS log: open non proxy file %s\n", fullpath);
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
	
	fprintf(logfd, "LancerFS log: cloudfs_write(path=%s, size=%zu,  \
						\n", fullpath, size);
	ret = pwrite(fileInfo->fh, buf, size, offset);	
	if(ret < 0){
     char errMsg[MAX_MSG_LEN];
     sprintf(errMsg, "cannot write file %s with offset %zu size %d at  \
							buf 0x%08x\n", fullpath, offset, size, buf);
     ret = cloudfs_error(errMsg);
	}
		
	int slave = -1;
	ret = lgetxattr(fullpath, "user.slave", &slave, sizeof(int));
	if(ret > 0){// set slave attribute before
			if(slave == 0){
				fprintf(logfd, "Lancer error: wrong slave id for proxy file %s\n",
								fullpath);
				goto error;
			}
			int dirty = 1;
			ret = lsetxattr(fullpath, "user.dirty", &dirty, sizeof(int), 0);
			if(ret < 0){
				fprintf(logfd, "Lancer eror: set dirty bit on %s failt\n",
								fullpath);
				goto error;	
			}	
	}

done:	
	return ret;
error:
	fprintf(logfd, "Unknown error in cloudfs_write\n");
	goto done;	
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
  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);	

	if(!get_proxy(fullpath)){
		//this is a local file
		struct stat buf;
		lstat(fullpath, &buf); 
		if(buf.st_size < _state.threshold){//small file, keep in SSD
			ret = close(fileInfo->fh);		
			goto done; 	
		}
		//push this file to cloud
		char cloudpath[MAX_PATH_LEN];
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
		cloudfs_generate_proxy(fullpath, &stat_buf);
	}else{// a proxy file	
			struct stat buf;							
      char slavepath[MAX_PATH_LEN+3];
      memset(slavepath, 0, MAX_PATH_LEN + 3);
      strcpy(slavepath, fullpath);
      cloud_slave_filename(slavepath);
		
		if(get_dirty(fullpath)){//dirty file, flush to Cloud
			cloud_push_shadow(fullpath, slavepath, &buf);		
		}
		
		//delete slave file	
		fclose(fileInfo->fh);
		unlink(slavepath);	
		//set proxy file attribute?
		set_dirty(fullpath, 0);
		set_slave(fullpath, 0); 
	}	
done:	
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
		.unlink					= cloudfs_unlink,
		.release				= cloudfs_release
		//TODO: utimens, mknod
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

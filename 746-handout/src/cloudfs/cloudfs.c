#include "cloudapi.h"
#include "cloudfs.h"
#include "dedup.h"

#define UNUSED __attribute__((unused))
#define DEBUG


static struct cloudfs_state _state;

void cloudfs_get_fullpath(const char *path, char *fullpath){ 
	sprintf(fullpath, "%s%s", _state.ssd_path, path);	
}

static int cloudfs_error(char *error_str)
{
    int retval = -errno;
    fprintf(stderr, "CloudFS Error: %s\n", error_str);

    /* FUSE always returns -errno to caller (yes, it is negative errno!) */
    return retval;
}

static int cloudfs_debug(char *err){
	int ret = 0;
	#ifdef DEBUG
	fprintf(stderr, "CloudFS Debug: %s\n", err);
	ret = -errno;	
	#endif
	return ret;
}

/*
 * Initializes the FUSE file system (cloudfs) by checking if the mount points
 * are valid, and if all is well, it mounts the file system ready for usage.
 *
 */
void *cloudfs_init(struct fuse_conn_info *conn UNUSED)
{
  cloud_init(_state.hostname);
  return NULL;
}

void cloudfs_destroy(void *data UNUSED) {
  cloud_destroy();
}

/*struct stat {
    dev_t     st_dev;      ID of device containing file 
    ino_t     st_ino;      inode number 
    mode_t    st_mode;     protection 
    nlink_t   st_nlink;    number of hard links 
    uid_t     st_uid;      user ID of owner 
    gid_t     st_gid;      group ID of owner 
    dev_t     st_rdev;     device ID (if special file) 
    off_t     st_size;     total size, in bytes 
    blksize_t st_blksize;  blocksize for file system I/O 
    blkcnt_t  st_blocks;   number of 512B blocks allocated 
    time_t    st_atime;    time of last access 
    time_t    st_mtime;    time of last modification 
    time_t    st_ctime;    time of last status change 
};
*/

int cloudfs_getattr(const char *path UNUSED, struct stat *statbuf UNUSED)
{
  int retval = 0;
		
  return retval;
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

int cloudfs_open(const char *path, struct fuse_file_info *fi){
	int ret = 0;
	int fd;
	char fullpath[MAX_PATH_LEN];

	cloudfs_get_fullpath(path, fullpath);

	char debugMsg[MAX_MSG_LEN];
	sprintf(debugMsg, "cfs_open(fullpath %s, fi = 0x%08x)\n", 
					fullpath, fi->flags);
	cloudfs_debug(debugMsg);
								
	fd = open(fullpath, fi->flags);
	if(fd < 0){
			char errMsg[MAX_MSG_LEN];
			sprintf(errMsg, "cannot open file %s\n", fullpath);	
			ret = cloudfs_error(errMsg);
	}	
	fi->fh = fd;
	return ret;		
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

	ret = pwrite(fileInfo->fh, buf, size, offset);
	if(ret < 0){
      char errMsg[MAX_MSG_LEN];
      sprintf(errMsg, "cannot write file %s with offset %zu size %d at buf 0x%08x\n", 
							fullpath, offset, size, buf);
      ret = cloudfs_error(errMsg);
	}
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

  int fuse_stat = fuse_main(argc, argv, &cloudfs_operations, NULL);
    
  return fuse_stat;
}

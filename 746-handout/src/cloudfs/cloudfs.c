#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <time.h>
#include <unistd.h>
#include "cloudapi.h"
#include "cloudfs.h"
#include "dedup.h"

#define UNUSED __attribute__((unused))
#define DEBUG


static struct cloudfs_state _state;

void cloudfs_get_fullpath(const char *path, char *fullpath){ 
	sprintf(fullpath, "%s%s", _state.ssd_path, path);	
}


static int UNUSED cloudfs_error(char *error_str)
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
    .readdir        = NULL,
    .destroy        = cloudfs_destroy
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

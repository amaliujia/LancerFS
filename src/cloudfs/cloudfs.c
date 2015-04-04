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

static struct cloudfs_state state_;


int cloudfs_error(char *error_str)
{
	int retval = -errno;
	fprintf(stderr, "CloudFS Error: %s\n", error_str);
	return retval;
}

void *cloudfs_init(struct fuse_conn_info *conn UNUSED)
{
	cloud_init(state_.hostname);
	return NULL;
}

void cloudfs_destroy(void *data UNUSED) {
	cloud_destroy();
}

int cloudfs_getattr(const char *path, struct stat *statbuf)
{
	return wgetattr(path, statbuf);
}

static 
struct fuse_operations cloudfs_operations; 

int cloudfs_start(struct cloudfs_state *state,
    const char* fuse_runtime_name) {

  cloudfs_operations.init           = cloudfs_init;
  cloudfs_operations.getattr        = cloudfs_getattr;
  cloudfs_operations.destroy        = cloudfs_destroy;

  int argc = 0;
  char* argv[10];
  argv[argc] = (char *) malloc(128 * sizeof(char));
  strcpy(argv[argc++], fuse_runtime_name);
  argv[argc] = (char *) malloc(1024 * sizeof(char));
  strcpy(argv[argc++], state->fuse_path);
	

	argv[argc++] = "-s"; // set the fuse mode to single thread
	//argv[argc++] = "-f"; // run fuse in foreground 

	state_  = *state;
	winit(state);
	int fuse_stat = fuse_main(argc, argv, &cloudfs_operations, NULL);

	return fuse_stat;
}


#ifndef __CLOUDFS_H_
#define __CLOUDFS_H_

#define MAX_PATH_LEN 4096
#define MAX_HOSTNAME_LEN 1024

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
#include <stdarg.h>

struct cloudfs_state {
  char ssd_path[MAX_PATH_LEN];
  char fuse_path[MAX_PATH_LEN];
  char hostname[MAX_HOSTNAME_LEN];
  int ssd_size;
  int threshold;
  int avg_seg_size;
  int rabin_window_size;
  char no_dedup;
};

int cloudfs_start(struct cloudfs_state* state,
                  const char* fuse_runtime_name);  
void cloudfs_get_fullpath(const char *path, char *fullpath);
void log_msg(const char *format, ...);
void cloud_filename(char *path);

#endif
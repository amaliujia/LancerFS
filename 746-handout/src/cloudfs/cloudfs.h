#ifndef __CLOUDFS_H_
#define __CLOUDFS_H_

#include "include.h"

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


//Tool function
int get_proxy(const char *path);
void print_cloudfs_state();
void cloudfs_log_init();
void cloudfs_log_init();


#endif
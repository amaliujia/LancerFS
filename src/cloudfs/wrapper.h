#ifndef __WRAPPER_H_
#define __WRAPPER_H_

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>


#define MAX_PATH_LEN 4096
#define MAX_HOSTNAME_LEN 1024

#ifdef __cplusplus 
extern "C"{
#endif

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

//init Lancerfs
void winit(struct cloudfs_state *state);
int wgetattr(const char *path, struct stat *statbuf);
int wmknod(const char *path, mode_t mode, dev_t dev);
int wmkdir(const char *path, mode_t mode);
int wrmdir(const char *path);
int wunlink(const char *path);
int wreaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
             struct fuse_file_info *fileInfo);
int wchmod(const char *path, mode_t mode);
int wchown(const char *path, uid_t uid, gid_t gid);
int wsetxattr(const char *path, const char *name,
              const char *value, size_t size, int flags);
int wgetxattr(const char *path, const char *name, char *value, size_t size);
int wrelease(const char *path, struct fuse_file_info *fileInfo);
int wopen(const char *path, struct fuse_file_info *fileInfo);
int wread(const char *path, char *buf, size_t size, off_t offset,
          struct fuse_file_info *fileInfo);
int wwrite(const char *path, const char *buf, size_t size, off_t offset,
           struct fuse_file_info *fileInfo);
int wopendir(const char *path, struct fuse_file_info *fileInfo);
int wtruncate(const char *path, off_t newSize);
int waccess(const char *path, int mask);
void wdestroy(void *data);
int wutimens(const char *path, const struct timespec tv[2]);
int wioctl(const char *fd, int cmd, void *arg ,
						struct fuse_file_info *info, unsigned int flags, void *data);

#ifdef __cplusplus 
}
#endif

#endif

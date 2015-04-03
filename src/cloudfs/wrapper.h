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

#include "Lancerfs.h"

#ifdef __cplusplus 
extern "C"{
#endif

int wgetattr(const char *path, struct stat *statbuf);
/*int wreadlink(const char *path, char *link, size_t size);
int wmknod(const char *path, mode_t mode, dev_t dev);
int wmkdir(const char *path, mode_t mode);
int wunlink(const char *path);*/
int wmkdir(const char *path, mode_t mode );
/*int wsymlink(const char *path, const char *link);
int wrename(const char *path, const char *newpath);
int wreaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo);
int wlink(const char *path, const char *newpath);
int wchmod(const char *path, mode_t mode);
int wchown(const char *path, uid_t uid, gid_t gid);*/
//int wtruncate(const char *path, off_t newSize);
/*int wutime(const char *path, struct utimbuf *ubuf);
int wopen(const char *path, struct fuse_file_info *fileInfo);
int wread(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo);
int wwrite(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo);
int wstatfs(const char *path, struct statvfs *statInfo);
int wflush(const char *path, struct fuse_file_info *fileInfo);
int wrelease(const char *path, struct fuse_file_info *fileInfo);
int wfsync(const char *path, int datasync, struct fuse_file_info *fi);
int wsetxattr(const char *path, const char *name, const char *value, size_t size, int flags);
int wgetxattr(const char *path, const char *name, char *value, size_t size);
int wlistxattr(const char *path, char *list, size_t size);
int wremovexattr(const char *path, const char *name);
int wopendir(const char *path, struct fuse_file_info *fileInfo);*/
int wreaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo);
/*int wreleasedir(const char *path, struct fuse_file_info *fileInfo);
int wfsyncdir(const char *path, int datasync, struct fuse_file_info *fileInfo);
int winit(struct fuse_conn_info *conn);*/
//int wtruncate(const char *path, off_t offset, struct fuse_file_info *fileInfo);

#ifdef __cplusplus 
}
#endif

#endif

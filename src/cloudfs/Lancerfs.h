#ifndef LANCER_FS_HPP
#define LANCER_FS_HPP

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
#include <stdarg.h>
#include <fuse.h>

#include <map>
#include <set>
#include <vector>
#include <iostream>

#include "libs3.h"
//#include "cloudapi.h"

#define MAX_PATH_LEN 4096
#define MAX_HOSTNAME_LEN 1024

class LancerFS{
private:

	static LancerFS *_lancerFS;
	
	FILE *logfd; 
			
	FILE *outfile;	
	FILE *infile;

public:
	
public:
	LancerFS();
	~LancerFS();

	static LancerFS *instance();

	//base 
	void cloudfs_get_fullpath(const char *path, char *fullpath);

	//log
	void log_msg(const char *format, ...);	
	int cloudfs_error(char *error_str);	

	//cloud	

	// FS API	
	int lgetattr(const char *path, struct stat *statbuf);
	int lmknod(const char *path, mode_t mode, dev_t dev);
	int lmkdir(const char *path, mode_t mode);
	int lunlink(const char *path);
	int lrmdir(const char *path);
	int llink(const char *path, const char *newpath);
	int lchmod(const char *path, mode_t mode);
	int lopen(const char *path, struct fuse_file_info *fileInfo);
	int lread(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo);
	int lwrite(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo);
	int lrelease(const char *path, struct fuse_file_info *fileInfo);
	int lsetxattr(const char *path, const char *name, const char *value, size_t size, int flags);
	int lgetxattr(const char *path, const char *name, char *value, size_t size);
	int lopendir(const char *path, struct fuse_file_info *fileInfo);
	int lreaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo);
	int ltruncate(const char *path, off_t offset, struct fuse_file_info *fileInfo);
};

#endif

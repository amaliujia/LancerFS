#ifndef LANCER_FS_HPP
#define LANCER_FS_HPP

//c
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

//c++
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <string>

#include "Fuse.h"
#include "duplication.h"

#define MAX_PATH_LEN 4096
#define MAX_HOSTNAME_LEN 1024

class LancerFS{
private:

	static LancerFS *_lancerFS;
	
	FILE *logfd; 
	char *logpath;

	duplication *dup;			

public:
	fuse_struct state_;	
	
public:
	LancerFS();
 	LancerFS(struct cloudfs_state *state);
	~LancerFS();
	static LancerFS *instance();

public:	
	// FS API	
	int cloudfs_getattr(const char *path, struct stat *statbuf);
	int cloudfs_mknod(const char *path, mode_t mode, dev_t dev);
	int cloudfs_mkdir(const char *path, mode_t mode);
	int cloudfs_unlink(const char *path);
	int cloudfs_rmdir(const char *path);
	int cloudfs_link(const char *path, const char *newpath);
	int cloudfs_chmod(const char *path, mode_t mode);
	int cloudfs_open(const char *path, struct fuse_file_info *fileInfo);
	int cloudfs_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fileInfo);
	int cloudfs_write(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fileInfo);
	int cloudfs_release(const char *path, struct fuse_file_info *fi);
	int cloudfs_setxattr(const char *path, const char *name, const char *value,
                         size_t size, int flags);
	int cloudfs_getxattr(const char *path, const char *name, char *value,
                         size_t size);
	int cloudfs_opendir(const char *path, struct fuse_file_info *fileInfo);
	int cloudfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fileInfo);
	int cloudfs_truncate(const char *path, off_t newsize);
	void *cloudfs_init(struct fuse_conn_info *conn);
	void cloudfs_destroy(void *data); 
	int cloudfs_utimens(const char *path, const struct timespec tv[2]);
	int cloudfs_access(const char *path, int mask);

private:
	//base 
	void cloudfs_get_fullpath(const char *path, char *fullpath);

	//log
	void log_msg(const char *format, ...);	
	int cloudfs_error(char *error_str);	
	void cloudfs_log_close();
	void cloudfs_log_init();

	//cloud	
//	int get_buffer(const char *buffer, int bufferLength); 
//	int put_buffer(char *buffer, int bufferLength); 
	void cloud_get_shadow(const char *fullpath, const char *cloudpath);
	void cloud_push_file(const char *fpath, struct stat *stat_buf);
	void cloud_push_shadow(const char *fullpath, const char *shadowpath,
                           struct stat *stat_buf);
	void cloud_filename(char *path);
	void cloud_slave_filename(char *path);

	//FS Kernel
	void cloudfs_generate_proxy(const char *fullpath, struct stat *buf);
	int cloudfs_save_attribute(const char *fullpath, struct stat *buf);
	void cloudfs_set_attribute(const char *fullpath, struct stat *buf);
	int get_proxy(const char *fullpath);
	int set_proxy(const char *fullpath, int proxy);
	int get_dirty(const char *fullpath);
	int	set_dirty(const char *fullpath, int dirty);
	int set_slave(const char *fullpath, int slave);
	int	get_slave(const char *fullpath);

};

#endif

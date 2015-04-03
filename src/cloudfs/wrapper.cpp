#include "wrapper.h"
#include "Lancerfs.h"

static LancerFS instance;

int wgetattr(const char *path, struct stat *statbuf){
	return instance.getattr(path, statbuf);
}

int wmkdir(const char *path, mode_t mode){
	return instance.mkdir(path, mode);
}

int wreaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
											struct fuse_file_info *fileInfo)
{
	return instance.readdir(path, buf, filler, offset, fileInfo);
}

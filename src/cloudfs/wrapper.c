#include "wrapper.h"

int getattr(const char *path, struct stat *statbuf){
	return LancerFS::instance()->getattr(path, statbuf);
}

int mkdir(const char *path, mode_t mode){
	return LancerFS::instance()->mkdri(path, mode);
}

int readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
											struct fuse_file_info *fileInfo)
{
	return LancerFS::instance()->readdir(path, buf, filler, offset, fileInfo);
}

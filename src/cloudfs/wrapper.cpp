#include "wrapper.h"
#include "Lancerfs.h"

static LancerFS instance;

void winit(struct cloudfs_state *state){
	instance.state_ = *state;
}

int wgetattr(const char *path, struct stat *statbuf){
	return instance.getattr(path, statbuf);
}

int wmkdir(const char *path, mode_t mode){
	return instance.mkdir(path, mode);
}

int wreaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
											struct fuse_file_info *fileInfo)
{
	return instance.lreaddir(path, buf, filler, offset, fileInfo);
}

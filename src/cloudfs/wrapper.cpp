#include "wrapper.h"
#include "Lancerfs.h"

static LancerFS *instance;

void winit(struct cloudfs_state *state){
	instance = new LancerFS(state);
}

int wgetattr(const char *path, struct stat *statbuf){
	return instance->cloudfs_getattr(path, statbuf);
}

int wmkdir(const char *path, mode_t mode){
	return instance->cloudfs_mkdir(path, mode);
}

int wreaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
											struct fuse_file_info *fileInfo)
{
	return instance->cloudfs_readdir(path, buf, filler, offset, fileInfo);
}

int wmknod(const char *path, mode_t mode, dev_t dev){
	return instance->cloudfs_mknod(path, mode, dev);
}

int wunlink(const char *path){
	return instance->cloudfs_unlink(path);
}

int wrmdir(const char *path){
	return instance->cloudfs_rmdir(path);
}

int wchmod(const char *path, mode_t mode){
	return instance->cloudfs_chmod(path, mode);
}

int wsetxattr(const char *path, const char *name, const char *value,
              size_t size, int flags){
	return instance->cloudfs_setxattr(path, name, value, size, flags);
}

int wgetxattr(const char *path, const char *name, char *value, size_t size){
	return instance->cloudfs_getxattr(path, name, value, size);
}

int wrelease(const char *path, struct fuse_file_info *fileInfo){
	return instance->cloudfs_release(path, fileInfo);
}

int wopen(const char *path, struct fuse_file_info *fileInfo){
	return instance->cloudfs_open(path, fileInfo);
}

int wread(const char *path, char *buf, size_t size, off_t offset,
          struct fuse_file_info *fileInfo)
{
	return instance->cloudfs_read(path, buf, size, offset, fileInfo);
}

int wwrite(const char *path, const char *buf, size_t size, off_t offset,
           struct fuse_file_info *fileInfo){
	return instance->cloudfs_write(path, buf, size, offset, fileInfo);
}

int wopendir(const char *path, struct fuse_file_info *fileInfo){
	return instance->cloudfs_opendir(path, fileInfo);
}

int wtruncate(const char *path, off_t newSize){
	 return instance->cloudfs_truncate(path, newSize);
}

void wdestroy(void *data){
	 instance->cloudfs_destroy(data);
}

int waccess(const char *path, int mask){
	return instance->cloudfs_access(path, mask);
}

int wutimens(const char *path, const struct timespec tv[2]){
	return instance->cloudfs_utimens(path, tv);
}

int wioctl(const char *fd, int cmd, void *arg, 
						struct fuse_file_info *info, unsigned int flags, void *data)
{
	return instance->cloudfs_ioctl(fd, cmd, arg, info, flags, data); 
}

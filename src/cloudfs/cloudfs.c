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

#include "cloudapi.h"
#include "cloudfs.h"
#include "dedup.h"


#define UNUSED __attribute__((unused))

static struct cloudfs_state state_;


int cloudfs_error(char *error_str) {
    int retval = -errno;
    fprintf(stderr, "CloudFS Error: %s\n", error_str);
    return retval;
}

void *cloudfs_init(struct fuse_conn_info *conn UNUSED) {
    cloud_init(state_.hostname);
    cloud_create_bucket("bkt");
    cloud_create_bucket("snapshot");
    return NULL;
}

void cloudfs_destroy(void *data UNUSED) {
    cloud_destroy();
}

int cloudfs_getattr(const char *path, struct stat *statbuf) {
    return wgetattr(path, statbuf);
}

int cloudfs_mkdir(const char *path, mode_t mode) {
    return wmkdir(path, mode);
}

int cloudfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info *fi) {
    return wreaddir(path, buf, filler, offset, fi);
}

int cloudfs_getxattr(const char *path, const char *name, char *value,
                     size_t size) {
    return wgetxattr(path, name, value, size);
}

int cloudfs_setxattr(const char *path, const char *name, const char *value,
                     size_t size, int flags) {
    return wsetxattr(path, name, value, size, flags);
}


int cloudfs_access(const char *path, int mask) {
    return waccess(path, mask);
}

int cloudfs_mknod(const char *path, mode_t mode, dev_t dev) {
    return wmknod(path, mode, dev);
}


int cloudfs_open(const char *path, struct fuse_file_info *fi) {
    return wopen(path, fi);
}

int cloudfs_read(const char *path, char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi) {
    return wread(path, buf, size, offset, fi);
}

int cloudfs_write(const char *path, const char *buf, size_t size, off_t offset,
                  struct fuse_file_info *fi) {
    return wwrite(path, buf, size, offset, fi);
}

int cloudfs_release(const char *path, struct fuse_file_info *fi) {
    return wrelease(path, fi);
}


int cloudfs_opendir(const char *path, struct fuse_file_info *fi) {
    return wopendir(path, fi);
}

int cloudfs_utimens(const char *path, const struct timespec tv[2]) {
    return wutimens(path, tv);
}


int cloudfs_chmod(const char *path, mode_t mode) {
    return wchmod(path, mode);
}

int cloudfs_unlink(const char *path) {
    return wunlink(path);
}

int cloudfs_rmdir(const char *path) {
    return wrmdir(path);
}

int cloudfs_truncate(const char *path, off_t newsize) {
    return wtruncate(path, newsize);
}

int cloudfs_ioctl(const char *fd, int cmd, void *arg,
                  struct fuse_file_info *info,
                  unsigned int flags, void *data) {
    return wioctl(fd, cmd, arg, info, flags, data);
}

static struct fuse_operations cloudfs_operations = {
        .init           = cloudfs_init,
        .getattr        = cloudfs_getattr,
        .mkdir          = cloudfs_mkdir,
        .readdir        = cloudfs_readdir,
        .destroy        = cloudfs_destroy,
        .getxattr       = cloudfs_getxattr,
        .setxattr       = cloudfs_setxattr,
        .access         = cloudfs_access,
        .mknod          = cloudfs_mknod,
        .open           = cloudfs_open,
        .read           = cloudfs_read,
        .write          = cloudfs_write,
        .release        = cloudfs_release,
        .opendir        = cloudfs_opendir,
        .utimens        = cloudfs_utimens,
        .chmod          = cloudfs_chmod,
        .unlink         = cloudfs_unlink,
        .rmdir          = cloudfs_rmdir,
        .truncate       = cloudfs_truncate,
        .ioctl          = cloudfs_ioctl
};

int cloudfs_start(struct cloudfs_state *state,
                  const char *fuse_runtime_name) {

    int argc = 0;
    char *argv[10];
    argv[argc] = (char *) malloc(128 * sizeof(char));
    strcpy(argv[argc++], fuse_runtime_name);
    argv[argc] = (char *) malloc(1024 * sizeof(char));
    strcpy(argv[argc++], state->fuse_path);


    argv[argc++] = "-s"; // set the fuse mode to single thread
    //argv[argc++] = "-f"; // run fuse in foreground

    state_ = *state;

    winit(state);
    int fuse_stat = fuse_main(argc, argv, &cloudfs_operations, NULL);

    return fuse_stat;
}


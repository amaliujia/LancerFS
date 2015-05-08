#ifndef LANCER_FS_HPP
#define LANCER_FS_HPP

#include "Fuse.h"
#include "duplication.h"
#include "snapshot.h"

class LancerFS{
private:
    
    static LancerFS *_lancerFS;
    
    //FILE *logfd;
    char *logpath;
    
    duplication *dup;
   	SnapshotManager *snapshotMgr; 
    set<string> superfiles;
			    
public:
    fuse_struct state_;
    
public:
    LancerFS();
    LancerFS(struct cloudfs_state *state);
    ~LancerFS();
    
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
    int cloudfs_ioctl(const char *fd, int cmd, void *arg,
                      struct fuse_file_info *info,
                      unsigned int flags, void *data);
 
private:
    //base
    void cloudfs_get_fullpath(const char *path, char *fullpath);
    
    //log
    int cloudfs_error(char *error_str);
    void cloudfs_log_close();
    
    //cloud
    void cloud_get_shadow(const char *fullpath, const char *cloudpath);
    void cloud_push_file(const char *fpath, struct stat *stat_buf);
    void cloud_push_shadow(const char *fullpath, const char *shadowpath,
                           struct stat *stat_buf);
    void cloud_filename(char *path);
    
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
    void write_size_proxy(const char *fullpath, int size);
    int get_size_proxy(const char *fullpath);
    void delete_proxy(const char *fullpath);
    void get_proxy_path(const char *fullpath, char *hubfile);

    //Snapshot
    void init_snapshot();
    void save_utime(const char *fpath, struct timespec times[2]);
    int set_utime(const char *fpath, struct timespec times[2]);

};

#endif

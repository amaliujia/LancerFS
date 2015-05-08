#include "Lancerfs.h"
#include "transmission.h"

#ifdef __cplusplus
extern "C"
{
#endif
    
#include "cloudapi.h"
#include "dedup.h"
#include "snapshot-api.h"
#ifdef __cplusplus
}
#endif

#define SSD_DATA_PATH "data/"
#define SNAPSHOT_PATH "/.snapshot"

LancerFS::LancerFS(struct cloudfs_state *state){
    state_.init(state);
    
    //create data directory into ssd path
    char fpath[MAX_PATH_LEN];
    sprintf(fpath, "%sdata", state_.ssd_path);
    mkdir(fpath, S_IRWXU | S_IRWXG | S_IRWXO);
    
    //init log
    logpath = "/tmp/cloudfs.log";
    log_init(logpath);
		log_msg("LancerFS log: filesystem start\n");
    
    //init deduplication layer
    dup = new duplication(&state_);
    
		//init snapshot layer
    init_snapshot();
}

/*
	init_snapshot - try to open .snapshot file. If this file does not exit,
 create it.
 */
void LancerFS::init_snapshot(){
    char fpath[MAX_PATH_LEN];
    cloudfs_get_fullpath(SNAPSHOT_PATH, fpath);
    
    int fd = open(fpath, O_RDONLY | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH);
    if(fd < 0){
        cloudfs_error("LancerFS error: fail to create snapshot file");
    }
    close(fd);
    snapshotMgr = new SnapshotManager(state_.ssd_path);
    strcpy(snapshotMgr->fuse_path, state_.fuse_path);
}

/*
 cloudfs_generate_proxy - When file is bigger than threshold,
 push file into cloud, use this function
 generate proxy file to represent cloud file.
 */
void LancerFS::cloudfs_generate_proxy(const char *fullpath, struct stat *buf){
    int fd = creat(fullpath, buf->st_mode);
    log_msg("LancerFS log: create proxy file\n");
    if(fd < 0){
        log_msg("LancerFS error: fail to create proxy file %s\n", fullpath);
    }
    int proxy = 1;
    lsetxattr(fullpath, "user.proxy", &proxy, sizeof(int), 0);
    lsetxattr(fullpath, "user.st_size", &(buf->st_size), sizeof(off_t), 0);
    lsetxattr(fullpath, "user.st_mtime", &(buf->st_mtime), sizeof(time_t), 0);
    close(fd);
}

/*
 write_size_proxy- write true size of a proxy file into hidden file.
 */
void LancerFS::write_size_proxy(const char *fullpath, int size){
    //get hubfile name
    char hubfile[MAX_PATH_LEN];
    get_proxy_path(fullpath, hubfile);
    
   	log_msg("write_size_proxy(fullpath=%sfullpath, size=%d)\n",
            fullpath, size);
    
    FILE *fp = fopen(hubfile, "w");
    fprintf(fp, "%d\n", size);
    fclose(fp);
}

/*
 get_size_proxy- read true size of a proxy file into hidden file.
 */
int LancerFS::get_size_proxy(const char *fullpath){
    //get hubfile name
    char hubfile[MAX_PATH_LEN];
    get_proxy_path(fullpath, hubfile);
   	log_msg("read_size_proxy(fullpath=%s, size=", fullpath);
    FILE *fp = fopen(hubfile, "r");
    int size = 0;
    int ret = fscanf(fp, "%d", &size);
    if(ret != 1){
        log_msg("LancerFS error: read wrong proxy file szie, argument wrong");
    }
    log_msg("%d\n", size);
    return size;
}

/*
 delete_proxy- delete hidden file of given proxy file.
 */
void LancerFS::delete_proxy(const char *fullpath){
    log_msg("delete_proxy(fullpath=%s)\n", fullpath);
    char hubfile[MAX_PATH_LEN];
    get_proxy_path(fullpath, hubfile);
    unlink(hubfile);
}

/*
 cloudfs_save_attribute - Save file attributes into extended attribute of
 proxy file.
 */
int LancerFS::cloudfs_save_attribute(const char *fullpath, struct stat *buf){
    int ret = 0;
    ret = lstat(fullpath, buf);
    if(ret < 0){
        log_msg("save attribute\n");
        return ret;
    }
    
    off_t size;
    ret = lgetxattr(fullpath, "user.st_size", &size, sizeof(off_t));
    if(ret > 0){
        log_msg("%s Save size attribute\n", fullpath);
        buf->st_size = size;
    }
    time_t time;
    ret = lgetxattr(fullpath, "user.st_mtime", &time, sizeof(time_t));
    if(ret > 0){
        log_msg("%s Save time attribute\n", fullpath);
        buf->st_mtime = time;
    }
    return ret;
}

/*
 cloudfs_set_attribut- save extend attributes like size and mtime.
 */
void LancerFS::cloudfs_set_attribute(const char *fullpath, struct stat *buf){
    lsetxattr(fullpath, "user.st_size", &(buf->st_size), sizeof(off_t), 0);
    lsetxattr(fullpath, "user.st_mtime", &(buf->st_mtime), sizeof(time_t), 0);
}

/*
 cloud_filenam - Get cloud filename.
 */
void LancerFS::cloud_filename(char *path){
    while(*path != '\0'){
        if(*path == '/'){
            *path = '+';
        }
        path++;
    }
}

/*
 cloudfs_get_fullpath - get full path in linux namespace by given path.
 */
void LancerFS::cloudfs_get_fullpath(const char *path, char *fullpath){
    sprintf(fullpath, "%s", state_.ssd_path);
    path++;
    sprintf(fullpath, "%s%s%s", fullpath, SSD_DATA_PATH, path);
}

void LancerFS::cloudfs_log_close(){
    log_destroy();
}

/*
 get_proxy - Get value of extend attribute: proxy.
 */
int LancerFS::get_proxy(const char *fullpath){
    string s(fullpath);
    int i = s.size() - 1;
    while (i >= 0) {
        if(s[i] == '/'){
            i++;
            break;
        }
        i--;
    }
    s.insert(i, 1, '.');
    i++;
    s.insert(i, 1, '+');
    
    int ret = access(s.c_str(), F_OK);
    return ret == 0 ? 1 : 0;
}

/*
 get_proxy_path - get hidden file of a proxy file.
 */
void LancerFS::get_proxy_path(const char *fullpath, char *hubfile){
    string s(fullpath);
    int i = s.size() - 1;
    while (i >= 0) {
        if(s[i] == '/'){
            i++;
            break;
        }
        i--;
    }
    s.insert(i, 1, '.');
    i++;
    s.insert(i, 1, '+');
    strcpy(hubfile, s.c_str());
}

/*
 set_proxy - create hidden file of a proxy file.
 */
int LancerFS::set_proxy(const char *fullpath, int proxy){
    
    if(proxy == 1){
        string s(fullpath);
        int i = s.size() - 1;
        while (i >= 0) {
            if(s[i] == '/'){
                i++;
                break;
            }
            i--;
        }
        s.insert(i, 1, '.');
        i++;
        s.insert(i, 1, '+');
        //create proxy hub file
        int fd = creat(s.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
        if(fd < 0){
            log_msg("LancerFS error: fail to create proxy hub file %s",
                    fullpath);
            return -1;
        }
    }
    return 0;
}

/*
 get_dirty - Get value of extend attribute: dirty.
 */
int LancerFS::get_dirty(const char *fullpath){
    int dirty = 0;
    lgetxattr(fullpath, "user.dirty", &dirty, sizeof(int));
    return dirty;
}

/*
 set_dirty - Get value of extend attribute: dirty.
 */
int LancerFS::set_dirty(const char *fullpath, int dirty){
    return lsetxattr(fullpath, "user.dirty", &dirty, sizeof(int), 0);
}


/*
 set_slave - Set value of extend attribute: slave.
 */
int LancerFS::set_slave(const char *fullpath, int slave){
    return lsetxattr(fullpath, "user.slave", &slave, sizeof(int), 0);
}

/*
 get_slave - Get value of extend attribute: slave.
 */
int LancerFS::get_slave(const char *fullpath){
    int slave;
    lgetxattr(fullpath, "user.slave", &slave, sizeof(int));
    return slave;
}

void *LancerFS::cloudfs_init(struct fuse_conn_info *conn UNUSED)
{
    return NULL;
}

void LancerFS::cloudfs_destroy(void *data UNUSED) {
    cloud_destroy();
}


int LancerFS::cloudfs_getxattr(const char *path, const char *name,
                               char *value, size_t size)
{
    int ret = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("\ncloudfs_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, \
            size = %d)\n", path, name, value, size);
    cloudfs_get_fullpath(path, fpath);
    
    ret = lgetxattr(fpath, name, value, size);
    if (ret < 0){
        ret = cloudfs_error("cfs_getxattr lgetxattr");
    }else{
        log_msg(" value = \"%s\"\n", value);
   	}
    return ret;
}

int LancerFS::cloudfs_setxattr(const char *path, const char *name,
                               const char *value, size_t size, int flags){
    int ret = 0;
    char fpath[MAX_PATH_LEN];
    log_msg("cloudfs_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", \
            size=%d, flags=0x%08x)\n", path, name, value, size, flags);
    cloudfs_get_fullpath(path, fpath);
    
    ret = lsetxattr(fpath, name, value, size, flags);
    if (ret < 0)
        ret = cloudfs_error("cfs_setxattr lsetxattr");
    
    return ret;
}


int LancerFS::cloudfs_access(const char *path, int mask)
{
    int ret = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("cfs_access(path=\"%s\", mask=0%o)\n", path, mask);
    cloudfs_get_fullpath(path, fpath);
    
    ret = access(fpath, mask);
    
    if(ret < 0)
        ret = cloudfs_error("cfs_access access");
    return ret;
}

int LancerFS::cloudfs_mknod(const char *path, mode_t mode, dev_t dev){
    int ret = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("mknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
            path, mode, dev);
    cloudfs_get_fullpath(path, fpath);
    
    if(S_ISREG(mode)){
        ret = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
        if(ret < 0)
            ret = cloudfs_error("mknod open\n");
        else{
            ret = close(ret);
            if(ret < 0)
                ret = cloudfs_error("mknod close\n");
        }
    }else if(S_ISFIFO(mode)){
        ret = mkfifo(fpath, mode);
        if(ret < 0)
            ret = cloudfs_error("mknod mkfifo\n");
    }else{
        ret = mknod(fpath, mode, dev);
        if(ret < 0)
            ret = cloudfs_error("mknod mknod\n");
    }
    
    return ret;
}

int LancerFS::cloudfs_open(const char *path, struct fuse_file_info *fi){
    int ret = 0;
    int fd;
     // open .snapshot in O_RDONLY mode.
    if(strcmp(path, SNAPSHOT_PATH) == 0){
        fd = open(path, O_RDONLY);
        fi->fh = fd;
        return ret;
    }
    
    char fpath[MAX_PATH_LEN];
    
    log_msg("cloudfs_open(path\"%s\", fi=0x%08x)\n", path, fi);
    cloudfs_get_fullpath(path, fpath);
    
    fd = open(fpath, fi->flags);
    if (fd < 0){
        ret = cloudfs_error("open fail\n");
        return ret;
    }
    
    fi->fh = fd;

		if(state_.no_dedup && get_proxy(fpath)){
			log_msg("cloudfs download file from cloud %s\n", fpath);
		  char cloudpath[MAX_PATH_LEN];
  		memset(cloudpath, 0, MAX_PATH_LEN);
  		strcpy(cloudpath, fpath);	
			cloud_filename(cloudpath);
			get_from_cloud("bkt", cloudpath, fpath);	
		}

    return ret;
}

int LancerFS::cloudfs_read(const char *path, char *buf, size_t size,
                           off_t offset, struct fuse_file_info *fi)
{
    int ret = 0;
    log_msg("read(path=\"%s\", buf=0x%08x, size=%d, \
            offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);
    
    //not allowed read .snapshot
    if(strcmp(path, SNAPSHOT_PATH) == 0){
        ret = 0;
        return ret;
    }
    
    char fpath[MAX_PATH_LEN];
    cloudfs_get_fullpath(path, fpath);
    if(get_proxy(fpath) && !state_.no_dedup){ //if file saved in cloud
        log_msg("segment read\n");
        ret = dup->offset_read(fpath, buf, size, offset);
    }else{
        log_msg("naive read\n");
        ret = pread(fi->fh, buf, size, offset);
    }
    if(ret < 0)
        ret = cloudfs_error("cfs_read read");
    return ret;
}

int LancerFS::cloudfs_write(const char *path, const char *buf, size_t size,
                            off_t offset, struct fuse_file_info *fi)
{
    int ret = 0;
    log_msg("cfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, \
            fi=0x%08x)\n", path, buf, size, offset, fi);
    
    if(strcmp(path, SNAPSHOT_PATH) == 0){
        ret = 0;
        return ret;
    }
    
    char fpath[MAX_PATH_LEN];
    cloudfs_get_fullpath(path, fpath);
    
   	struct timespec tv[2];
    save_utime(fpath, tv);
    ret = pwrite(fi->fh, buf, size, offset);
    
    if(ret < 0){
        ret = cloudfs_error("pwrite fail\n");
        return ret;
    }
    set_utime(fpath, tv);
    if(get_proxy(fpath)){
        log_msg("dirty file %s\n", fpath);
   	}
    return ret;
}

int LancerFS::cloudfs_release(const char *path, struct fuse_file_info *fi){
    int ret = 0;
    if(strcmp(path, SNAPSHOT_PATH) == 0){
        ret = close(fi->fh);
        return ret;
    }
    
    char fullpath[MAX_PATH_LEN];
    cloudfs_get_fullpath(path, fullpath);
    log_msg("cloudfs_release(path=\"%s\")\t\t", path);
    if(!get_proxy(fullpath)){
        struct stat stat_buf;
        lstat(fullpath, &stat_buf);
        
        //this is a local file
        if(stat_buf.st_size < state_.threshold){//small file, keep in SSD
            log_msg("LancerFS log: close small file\n");
        }else{
            log_msg("LancerFS log: handle file whose size is over threshold\n");
            
            struct timespec tv[2];
            save_utime(fullpath, tv);
            
            dup->deduplicate(fullpath);
            truncate(fullpath, 0);
            write_size_proxy(fullpath, stat_buf.st_size);
           	dup->back_up(fullpath);
            set_utime(fullpath, tv);
        }
    }else{// a proxy file
        log_msg("LancerFS log: release big file: %s\n", path);
        
        struct stat buf;
        lstat(fullpath, &buf);
        
        struct timespec tv[2];
        save_utime(fullpath, tv);
        
        //TODO: release ownership of cache.
        if(get_dirty(fullpath)){
            dup->clean(fullpath);
        }else{
            dup->clean_cache();
        }
        
        set<string>::iterator iter;
        string s(fullpath);
        iter = superfiles.find(s);
        if(iter != superfiles.end()){
            superfiles.erase(iter);
        }
        
        truncate(fullpath, 0);
        write_size_proxy(fullpath, buf.st_size);
        set_utime(fullpath, tv);
    }
    ret = close(fi->fh);
    return ret;
}



int LancerFS::cloudfs_opendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp;
    int ret = 0;
    char fpath[PATH_MAX];
    
    log_msg("\ncfs_opendir(path=\"%s\", fi=0x%08x)\n", path, fi);
    cloudfs_get_fullpath(path, fpath);
    
    dp = opendir(fpath);
    if (dp == NULL)
        ret = cloudfs_error("opendir fail\n");
    
    fi->fh = (intptr_t) dp;
    return ret;
}

int LancerFS::cloudfs_utimens(const char *path, const struct timespec tv[2]){
    int ret = 0;
    char fullpath[MAX_PATH_LEN];
    cloudfs_get_fullpath(path, fullpath);
    
    log_msg("Lancer log: cfs_utimens(path=%s)\n", fullpath);
    ret = utimensat(0, fullpath, tv, 0);
    if(ret < 0){
        ret = cloudfs_error("utimes fail\n");
    }
    return ret;
}

/*
 Save mtime of file.
 */
void LancerFS::save_utime(const char *fpath, struct timespec times[2]){
    struct stat buf;
    lstat(fpath, &buf);
    
    times[0].tv_sec = buf.st_atime;
    times[1].tv_sec = buf.st_mtime;
    
    times[0].tv_nsec = 0;
    times[1].tv_nsec = 0;
    
    return;
}

/*
 Set mtime of file.
 */
int LancerFS::set_utime(const char *fpath, struct timespec times[2]){
    int ret = 0;
    ret = utimensat(0, fpath, times, 0);
    return ret;
}

int LancerFS::cloudfs_chmod(const char *path, mode_t mode)
{
    int ret = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("\ncfs_chmod(fpath=\"%s\", mode=0%03o)\n", path, mode);
    cloudfs_get_fullpath(path, fpath);
    
    ret = chmod(fpath, mode);
    if(ret < 0)
        ret = cloudfs_error("chmod fail\n");
    
    return ret;
}

int LancerFS::cloudfs_unlink(const char *path)
{
    int retstat = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("cloudfs_unlink(path=\"%s\")\n",
            path);
    if(strcmp(path, ".snapshot") == 0){
        retstat = -1;
        return retstat;
    }
    
    cloudfs_get_fullpath(path, fpath);
    if(dup->contain(fpath)){//delete metadata of cloud file.
        log_msg("remove big file\n");
        delete_proxy(fpath);
        dup->remove(fpath);
    }
    
    retstat = unlink(fpath);
    if (retstat < 0)
        retstat = cloudfs_error("unlink fail");
    
    return retstat;
}

int LancerFS::cloudfs_rmdir(const char *path){
    int retstat = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("\ncfs_rmdir(path=\"%s\")\n", path);
    cloudfs_get_fullpath(path, fpath);
    
    retstat = rmdir(fpath);
    if (retstat < 0)
        retstat = cloudfs_error("cfs_rmdir rmdir");
    
    return retstat;
}

int LancerFS::cloudfs_truncate(const char *path, off_t newsize)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\ncloudfs_truncate(path=\"%s\", newsize=%lld)\n",
            path, newsize);
    cloudfs_get_fullpath(path, fpath);
    
    retstat = truncate(fpath, newsize);
    if(retstat < 0)
        cloudfs_error("cloudfs_truncate truncate");
    
    return retstat;
}

int LancerFS::cloudfs_error(char *error_str)
{
    int retval = -errno;
   	log_msg(error_str);
    return retval;
}


LancerFS::~LancerFS(){
    cloudfs_log_close();
    
    //memory management
    delete dup;
    delete snapshotMgr;
}

int LancerFS::cloudfs_getattr(const char *path, struct stat *statbuf){
    int ret = 0;
    char fpath[MAX_PATH_LEN];
    cloudfs_get_fullpath(path, fpath);
    
    log_msg("\ncloudfs_getattr(path=\"%s\")\n",
            path);
    
    
    if(get_proxy(fpath)){//show extend attributes.
        ret = lstat(fpath, statbuf);
        statbuf->st_size = get_size_proxy(fpath);
        if(ret != 0){
            ret = cloudfs_error("getattr lstat\n");
        }
    }else{
        log_msg("cfs_getattr_smallfile(path=\"%s\"\n", fpath);
        ret = lstat(fpath, statbuf);
        if(ret != 0){
            ret = cloudfs_error("getattr lstat\n");
        }
    }
    return ret;
}

int LancerFS::cloudfs_readdir(const char *path, void *buf,
                              fuse_fill_dir_t filler, off_t offset,
                              struct fuse_file_info *fi)
{
    int ret = 0;
    DIR *dp;
    struct dirent *de;
    
    log_msg("\ncfs_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x,  \
            offset=%lld, fi=0x%08x)\n", path, buf, filler, offset, fi);
    dp = (DIR *)(uintptr_t) fi->fh;
    
    de = readdir(dp);
    if(de == 0){
        cloudfs_error("readdir fail\n");
        return ret;
    }
    
    do{
        //do not show lost+found.
        if(strcmp(de->d_name, "lost+found") == 0){
            continue;
        }
        log_msg("calling filler with name %s\n", de->d_name);
        if(filler(buf, de->d_name, NULL, 0) != 0){
            return -ENOMEM;
        }
    }while((de = readdir(dp)) != NULL);
    
    return ret;
}

int LancerFS::cloudfs_mkdir(const char *path, mode_t mode){
    int ret = 0;
    char fpath[MAX_PATH_LEN];
    cloudfs_get_fullpath(path, fpath);
    
    log_msg("\ncfd_mkdir(path=\"%s\", mode=0%3o)\n",
            path, mode);
    
    ret = mkdir(fpath, mode);
    if(ret < 0){
        ret = cloudfs_error("mkdir fail\n");
    }
    return ret;
}

int LancerFS::cloudfs_ioctl(const char *fd, int cmd, UNUSED void *arg,
                            UNUSED struct fuse_file_info *info,
                            UNUSED unsigned int flags,
                            void *data)
{
		if(fd == NULL){
			return -1;
		}	

    if(cmd == CLOUDFS_SNAPSHOT){
        log_msg("\nsnapshot make %lu\n", *(TIMESTAMP *)data);
        dup->increment();
        *(TIMESTAMP *)data = snapshotMgr->snapshot();
    }else if(cmd == CLOUDFS_RESTORE){
        TIMESTAMP t = *(TIMESTAMP *)data;
        log_msg("\nsnapshot restore %lu\n", t);
        snapshotMgr->restore(t);
        // duduplication layer should recover
        dup->recovery();
    }else if(cmd == CLOUDFS_DELETE){
        TIMESTAMP t = *(TIMESTAMP *)data;
        log_msg("\nsnapshot delete %lu\n", t);
        snapshotMgr->deletes(t);
    }else if(cmd == CLOUDFS_SNAPSHOT_LIST){
        snapshotMgr->list();
    }else{
        return -1;
    }
    return 0;
}


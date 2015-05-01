#include "Lancerfs.h"

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

#define UNUSED __attribute__((unused))
#define SSD_DATA_PATH "data/"
#define SNAPSHOT_PATH "/.snapshot"

LancerFS::LancerFS(struct cloudfs_state *state){
    state_.init(state);
    
    //create data directory into ssd path
    char fpath[MAX_PATH_LEN];
    sprintf(fpath, "%sdata", state_.ssd_path);
    mkdir(fpath, S_IRWXU | S_IRWXG | S_IRWXO);
    
    logpath = "/tmp/cloudfs.log";
    //init log
    logfd = fopen(logpath, "w");
    //setvbuf(logfd, NULL, _IOLBF, 0);
    if(logfd == NULL){
        printf("LancerFS Error: connot find log file\n");
        //exit(1);
    }
    log_msg("LancerFS log: filesystem start\n");
    
    //init deduplication layer
    dup = new duplication(logfd, &state_);
    
    //init snapshot layer
    init_snapshot();
}

/*
	init_snapshot - try to open .snapshot file. If this file does not exit, create it.	
*/
void LancerFS::init_snapshot(){
    char fpath[MAX_PATH_LEN];
    cloudfs_get_fullpath(SNAPSHOT_PATH, fpath);
    
    //FILE *fp = fopen(fpath, "ab+");
    //fclose(fp);
    int fd = open(fpath, O_RDONLY | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH);
    if(fd < 0){
        cloudfs_error("LancerFS error: fail to create snapshot file");
    }
    close(fd);
    snapshotMgr = new SnapshotManager(state_.ssd_path);
    //strcpy(snapshotMgr->ssd_path, state_.ssd_path);
    strcpy(snapshotMgr->fuse_path, state_.fuse_path);
		snapshotMgr->logfd = logfd;
}

/*
    When file is bigger than threshold, push file into cloud, use this function
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

void LancerFS::write_size_proxy(const char *fullpath, int size){
    //get hubfile name
    char hubfile[MAX_PATH_LEN];
    get_proxy_path(fullpath, hubfile);
    
    int fd = creat(hubfile, S_IRWXU | S_IRWXG | S_IRWXO);
    if(fd < 0){
        log_msg("LancerFS error: fail to create proxy hub file %s",
                fullpath);
        return; 
    }
    
    FILE *fp = fopen(hubfile, "w");
    fprintf(fp, "%d\n", size);
    fclose(fp);
}

int LancerFS::get_size_proxy(const char *fullpath){
    //get hubfile name
    char hubfile[MAX_PATH_LEN];
    get_proxy_path(fullpath, hubfile);
    
    FILE *fp = fopen(hubfile, "r");
    int size = 0;
    int ret = fscanf(fp, "%d", &size);
    if(ret != 1){
			log_msg("LancerFS error: read wrong proxy file szie, argument wrong");
		}
    return size;
}

void LancerFS::delete_proxy(const char *fullpath){
    char hubfile[MAX_PATH_LEN];
    get_proxy_path(fullpath, hubfile);
    unlink(hubfile);
}

/*
    Save file attributes into extended attribute of proxy file.
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


void LancerFS::cloudfs_set_attribute(const char *fullpath, struct stat *buf){
    lsetxattr(fullpath, "user.st_size", &(buf->st_size), sizeof(off_t), 0);
    lsetxattr(fullpath, "user.st_mtime", &(buf->st_mtime), sizeof(time_t), 0);
}

/*
    Get cloud filename.
 */
void LancerFS::cloud_filename(char *path){
    while(*path != '\0'){
        if(*path == '/'){
            *path = '+';
        }
        path++;
    }
}

void LancerFS::cloud_slave_filename(char *path){
    sprintf(path, "%s%s", path, ".sl");
}

void LancerFS::cloudfs_get_fullpath(const char *path, char *fullpath){
    sprintf(fullpath, "%s", state_.ssd_path);
    path++;
    sprintf(fullpath, "%s%s%s", fullpath, SSD_DATA_PATH, path);
}

void LancerFS::log_msg(const char *format, ...){
    va_list ap;
    va_start(ap, format);
    vfprintf(logfd, format, ap);
    fflush(logfd);
}

void LancerFS::cloudfs_log_close(){
    fclose(logfd);
}

void LancerFS::cloudfs_log_init(){
    logfd = fopen(logpath, "w");
    setvbuf(logfd, NULL, _IOLBF, 0);
    if(logfd == NULL){
        printf("LancerFS Error: connot find log file\n");
    }
}

/*
    Get value of extend attribute: proxy.
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
    
    int ret = access(s.c_str(), F_OK);
    return ret == 0 ? 1 : 0;
}


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
    strcpy (hubfile, s.c_str());
}

/*
    Set value of extend attribute: proxy.
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
        
        //create proxy hub file
        int fd = creat(s.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
        if(fd < 0){
            log_msg("LancerFS error: fail to create proxy hub file %s",
                    fullpath);
            return -1;
        }
    }
    return 0;
    
    //return lsetxattr(fullpath, "user.proxy", &proxy, sizeof(int), 0);
}

/*
    Get value of extend attribute: dirty.
 */
int LancerFS::get_dirty(const char *fullpath){
    int dirty = 0;
    lgetxattr(fullpath, "user.dirty", &dirty, sizeof(int));
    return dirty;
}

/*
    Get value of extend attribute: dirty.
 */
int LancerFS::set_dirty(const char *fullpath, int dirty){
    return lsetxattr(fullpath, "user.dirty", &dirty, sizeof(int), 0);
}


/*
    Set value of extend attribute: slave.
 */
int LancerFS::set_slave(const char *fullpath, int slave){
    return lsetxattr(fullpath, "user.slave", &slave, sizeof(int), 0);
}

/*
 Get value of extend attribute: slave.
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
    
    log_msg("\ncfs_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, \
            size = %d)\n", path, name, value, size);
    cloudfs_get_fullpath(path, fpath);
    
    ret = lgetxattr(fpath, name, value, size);
    if (ret < 0){
        ret = cloudfs_error("cfs_getxattr lgetxattr");
    }else{
        log_msg("    value = \"%s\"\n", value);
   	}
    return ret;
}

int LancerFS::cloudfs_setxattr(const char *path, const char *name,
                               const char *value, size_t size, int flags){
    int ret = 0;
    char fpath[MAX_PATH_LEN];
    log_msg("\ncfs_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", \
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
    
    log_msg("\ncfs_access(path=\"%s\", mask=0%o)\n", path, mask);
    cloudfs_get_fullpath(path, fpath);
    
    ret = access(fpath, mask);
    
    if(ret < 0)
        ret = cloudfs_error("cfs_access access");
    return ret;
}

int LancerFS::cloudfs_mknod(const char *path, mode_t mode, dev_t dev){
    int ret = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("\nmknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
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
    
    if(strcmp(path, SNAPSHOT_PATH) == 0){
        fd = open(path, O_RDONLY);
        fi->fh = fd;
        return ret;
    }
    
    char fpath[MAX_PATH_LEN];
    
    log_msg("\ncfd_open(path\"%s\", fi=0x%08x)\n", path, fi);
    cloudfs_get_fullpath(path, fpath);
    
    fd = open(fpath, fi->flags);
    if (fd < 0){
        ret = cloudfs_error("open fail\n");
        return ret;
    }
    
    fi->fh = fd;
    return ret;
}

int LancerFS::cloudfs_read(const char *path, char *buf, size_t size,
                           off_t offset, struct fuse_file_info *fi)
{
    int ret = 0;
    log_msg("\ncfs_superfiles.insert(s);read(path=\"%s\", buf=0x%08x, size=%d, \
            offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);
    
    if(strcmp(path, SNAPSHOT_PATH) == 0){
        ret = 0;
        return ret;
    }
    
    char fpath[MAX_PATH_LEN];
    cloudfs_get_fullpath(path, fpath);
    int s = dup->get_file_size(fpath);
    if(s > state_.threshold){
        ret = dup->offset_read(fpath, buf, size, offset);
    }else{
        ret = pread(fi->fh, buf, size, offset);
        if(ret < 0)
            ret = cloudfs_error("cfs_read read");
    }
    return ret;
}

int LancerFS::cloudfs_write(const char *path, const char *buf, size_t size,
                            off_t offset, struct fuse_file_info *fi)
{
    int ret = 0;
    log_msg("\ncfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, \
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
		set_utime(fpath, tv);
    if(ret < 0){
        ret = cloudfs_error("pwrite fail\n");
        
        return ret;
    }
 
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
    log_msg("\ncfs_release(path=\"%s\", fi=0x%08x)\n", path, fi);
    if(!get_proxy(fullpath)){
        struct stat stat_buf;
        lstat(fullpath, &stat_buf);

				//this is a local file
        log_msg("LancerFS log: release local file\n");
        if(stat_buf.st_size < state_.threshold){//small file, keep in SSD
            log_msg("LancerFS log: close local file\n");
        }else{
            dup->deduplicate(fullpath);
            
            struct timespec tv[2];
            save_utime(fullpath, tv);
    
            truncate(fullpath, 0);
            write_size_proxy(fullpath, stat_buf.st_size);
            
            //cloudfs_generate_proxy(fullpath, &stat_buf);
            
            set_utime(fullpath, tv);
        }
    }else{// a proxy file
        log_msg("LancerFS log: handle proxy file\n");

//        struct stat buf;
//        lstat(fullpath, &buf);
        
//        struct timespec tv[2];
//        save_utime(fullpath, tv);
        
//        set<string>::iterator iter;
//        string s(fullpath);
//        iter = superfiles.find(s);
//        if(iter != superfiles.end()){
//            superfiles.erase(iter);
//        }
        
        //truncate(fullpath, 0);
        //write_size_proxy(fullpath, buf.st_size);
//        set_utime(fullpath, tv);
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
    lsetxattr(fullpath, "user.st_mtime", &tv[1], sizeof(time_t), 0);
    if(ret < 0){
        ret = cloudfs_error("utimes fail\n");
    }
    return ret;
}

void LancerFS::save_utime(const char *fpath, struct timespec times[2]){
	struct stat buf;
	lstat(fpath, &buf);
		
	times[0].tv_sec = buf.st_atime;
	times[1].tv_sec = buf.st_mtime;
	
	times[0].tv_nsec = 0;
	times[1].tv_nsec = 0;
	
	return;
}

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
    
    log_msg("cfs_unlink(path=\"%s\")\n",
            path);
    if(strcmp(path, ".snapshot") == 0){
        retstat = -1;
        return retstat;
    }

    cloudfs_get_fullpath(path, fpath);
    
    if(dup->contain(fpath)){
        dup->remove(fpath);
				delete_proxy(fpath);
    }
    retstat = unlink(fpath);
    if (retstat < 0)
        retstat = cloudfs_error("unlink fail");
    
    return retstat;
}

int LancerFS::cloudfs_rmdir(const char *path){
    int retstat = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("cfs_rmdir(path=\"%s\")\n", path);
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
    if (retstat < 0)
        cloudfs_error("cloudfs_truncate truncate");
    
    return retstat;
}

int LancerFS::cloudfs_error(char *error_str)
{
    int retval = -errno;
   	log_msg(error_str);
    return retval;
}

LancerFS::LancerFS(){
    logpath = "/tmp/cloudfs.log";
    
    //init log
    logfd = fopen(logpath, "w");
    setvbuf(logfd, NULL, _IOLBF, 0);
    if(logfd == NULL){
        printf("LancerFS Error: connot find log file\n");
        //exit(1);
    }
    log_msg("LancerFS log: filesystem start\n");
    //init deduplication layer
    dup = new duplication(logfd, state_.ssd_path);
}

LancerFS::~LancerFS(){
    fclose(logfd);
    
    //memory management
    delete dup;
		delete snapshotMgr;
}

int LancerFS::cloudfs_getattr(const char *path, struct stat *statbuf){
    int ret = 0;
    char fpath[MAX_PATH_LEN];
    cloudfs_get_fullpath(path, fpath);
    
    log_msg("\ncfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
            path, statbuf);
    
    if(dup->contain(fpath)){
        ret = lstat(fpath, statbuf);
        log_msg("\ncfs_getattr_proxy(path=\"%s\"\n", fpath);
        statbuf->st_size = get_size_proxy(fpath);
        if(ret != 0){
            ret = cloudfs_error("getattr lstat\n");
        }
    }else{
        log_msg("\ncfs_getattr_local(path=\"%s\"\n", fpath); 
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

int LancerFS::cloudfs_ioctl(const char *fd, int cmd, void *arg,
													struct fuse_file_info *info, unsigned int flags, void *data)
{
	if(cmd == CLOUDFS_SNAPSHOT){
		dup->increment();
		*(TIMESTAMP *)data = snapshotMgr->snapshot();
		log_msg("snapshot make %lu", *(TIMESTAMP *)data);
	}else if(cmd == CLOUDFS_RESTORE){
		TIMESTAMP t = *(TIMESTAMP *)data;
		log_msg("snapshot restore %lu", t);
		snapshotMgr->restore(t);
		// duduplication layer should recover
		dup->recovery();	
	}else if(cmd == CLOUDFS_DELETE){
		TIMESTAMP t = *(TIMESTAMP *)data;
		snapshotMgr->deletes(t);
	}else if(cmd == CLOUDFS_SNAPSHOT_LIST){
		snapshotMgr->list();
	}else{
		return -1;
	} 
	return 0;
}


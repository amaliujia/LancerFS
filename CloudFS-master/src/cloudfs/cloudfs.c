#define _GNU_SOURCE
#include <assert.h>
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
#include <openssl/md5.h>
#include "cloudapi.h"

#include "dedup.h"
#include "extmap.h"
#include "cloudfs.h"
#include "zlib.h"


#define UNUSED __attribute__((unused))

typedef int bool;
#define true 1
#define false 0
// #define PATH_MAX 200
#define READ 1
#define WRITE 2
#define CLOSED 0

static struct cloudfs_state state_;
// static StrMap *sm;
static int window_size = 48;
static int avg_seg_size = 4096;
static int min_seg_size = 2048;
static int max_seg_size = 8192;
int cur_cache_capacity = 0;
FILE * logfile;
char md5_backup_fpath[MAX_PATH_LEN];
char cache_backup_fpath[MAX_PATH_LEN];

struct file_info {
    char path[MAX_PATH_LEN];
    struct chunk *chunk_list;
    struct file_info *next;
    int open_cnt;
};

struct chunk {
    int size;
    char md5[(MD5_DIGEST_LENGTH + 1) * 2];
    struct chunk *next;
};

struct file_info *head_node = NULL;

void fullpath(const char *path, char fpath[])
{
    char p[PATH_MAX];
    strcpy(p, path + 1);
    strcpy(fpath, state_.ssd_path);
    strcat(fpath, p);
    return;
}

void full_cache_path(char path[], char cache_fpath[]){
    strcpy(cache_fpath, state_.ssd_path);
    strcat(cache_fpath, ".cache/");
    strcat(cache_fpath, path);
    return;
}

int list_bucket(const char *key, time_t modified_time, uint64_t size)
{
    fprintf(stdout, "%s %lu %llu\n", key, modified_time, size);
    return 0;
}

int list_service(const char *bucketName)
{
    fprintf(stdout, "%s\n", bucketName);
    return 0;
}

static FILE *outfile;
int get_buffer(const char *buffer, int bufferLength)
{
    int retval;
    retval = fwrite(buffer, 1, bufferLength, outfile);
    return retval;
}

static FILE *infile;
int put_buffer(char *buffer, int bufferLength)
{
    int retval;
    fprintf(stdout, "put_buffer %d \n", bufferLength);
    retval = fread(buffer, 1, bufferLength, infile);
    return retval;
}

void print_chunk_list(struct chunk *chunk_head)
{
    int i = 0;
    while (chunk_head) {
        fprintf(logfile, "[%3d] %4d %s\n", i++, chunk_head->size, chunk_head->md5);
        chunk_head = chunk_head->next;
    }
}

void print_file_info_list()
{
    fprintf(logfile, "++++++++++++++++++++++++++++++++++\n");
    struct file_info *file_info_ptr = head_node;
    while (file_info_ptr) {
        fprintf(logfile, "%s\n", file_info_ptr->path);
        print_chunk_list(file_info_ptr->chunk_list);
        fprintf(logfile, "----------------------------------\n");
        file_info_ptr = file_info_ptr->next;
    }
    fprintf(logfile, "++++++++++++++++++++++++++++++++++\n");
}

//segment the new written file with rabin lib then update the chunk list
struct chunk *update_chunk(struct file_info *curr_file, struct chunk *curr_chunk){
    rabinpoly_t *rp = rabin_init(window_size, avg_seg_size, min_seg_size, max_seg_size);
    if (!rp) {
        fprintf(logfile, "Failed to init rabinhash algorithm\n");
    }

    MD5_CTX ctx;
    unsigned char md5[MD5_DIGEST_LENGTH];
    int i;
    for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
        md5[i] = '\0';
    }
    struct chunk *new_chunk_list, *curr_chunk_ptr;
    new_chunk_list = NULL;
    curr_chunk_ptr = NULL;
    char fpath[MAX_PATH_LEN];
    char fpath_nosuffix[MAX_PATH_LEN];
    char curr_chunk_fpath[MAX_PATH_LEN];
    if(curr_chunk){
        fullpath(curr_chunk->md5, curr_chunk_fpath);
    }
    int new_segment = 0;
    int len, segment_len = 0, b;
    char buf[max_seg_size]; //TODO, use global variables, don't hard code!
    int bytes;
    int fd;
    int retval = -1;
    struct stat statbuf;
    FILE *infile_to_compress = NULL;
    MD5_Init(&ctx);
    strcpy(fpath, curr_file -> path);
    strcpy(fpath_nosuffix, curr_file -> path);
    //get the file statbuf before rename
    retval = lgetxattr(fpath_nosuffix, "user.stat", &statbuf, sizeof(struct stat));
    if(retval < 0){
        perror("update_chunk: get user.stat error!\n");
    }


    if(!curr_chunk){
        fprintf(logfile, "update new chunk$$\n");
        strcat(fpath, "$");
        fd = open(fpath, O_RDWR);
    }
    else{
        //chunks get from cloud
        fd = open(curr_chunk_fpath, O_RDWR);
        statbuf.st_size -= curr_chunk->size;
    }
    
    char chunk_MD5[MD5_DIGEST_LENGTH * 2 + 2];
    while ((bytes = read(fd, buf, sizeof buf)) > 0) {
        char *buftoread = (char *) &buf[0];
        while ((len =
                rabin_segment_next(rp,
                                   buftoread +
                                   segment_len, bytes,
                                   &new_segment)) > 0) {
            MD5_Update(&ctx, buftoread, len);
            segment_len += len;

            if (new_segment) {
                fprintf(logfile, "new segment!\n");
                MD5_Final(md5, &ctx);
                //update the strmap map
                //key: MD5; value: dir
                for (b = 0; b < MD5_DIGEST_LENGTH; b++) {
                    fprintf(logfile, "%02x", md5[b]);
                    sprintf(&chunk_MD5[b * 2], "%02x", md5[b]);
                }
                if(curr_chunk_ptr){ //has been initialized and extended
                    curr_chunk_ptr -> next = (struct chunk *)malloc(sizeof(struct chunk));
                    curr_chunk_ptr = curr_chunk_ptr -> next;
                }
                else{
                    curr_chunk_ptr = (struct chunk *)malloc(sizeof(struct chunk));
                    new_chunk_list = curr_chunk_ptr;
                }
                curr_chunk_ptr -> next = NULL;
                strcpy(curr_chunk_ptr -> md5, chunk_MD5);
                curr_chunk_ptr -> size = segment_len;
                //update the file size! TODO  
                statbuf.st_size += segment_len;
                retval = lsetxattr(fpath_nosuffix, "user.stat", &statbuf, sizeof(struct stat), 0);
                if(retval < 0){
                    perror("update_chunk: update user.stat failed!\n");
                }


                if (!exist(chunk_MD5)) {
                    fprintf(logfile, "not exists in strmap!\n");
                    insert(chunk_MD5, 1);
                    fprintf(logfile, "md5_map_it:\n");
                    md5_map_it(md5_backup_fpath);
                    //put the segment onto the cloud, named after MD5
                    infile_to_compress = fmemopen(buftoread, segment_len, "r");
                    char infile_tmp_dir[MAX_PATH_LEN];
                    fullpath("/comp_tmp", infile_tmp_dir);
                    infile = fopen(infile_tmp_dir, "w+");
                    retval = def(infile_to_compress, infile, segment_len, Z_DEFAULT_COMPRESSION);
                    if(retval != Z_OK){
                        perror("compression failed!\n");
                    }

                    fprintf(logfile, "key: %s", chunk_MD5);
                    fprintf(logfile, " ");
                    fprintf(logfile, "value: %d\n", get(chunk_MD5));

                    fseek(infile, 0, SEEK_END);
                    int upload_len = ftell(infile);
                    fseek(infile, 0, SEEK_SET);
                    cloud_put_object("cloudfs",
                                     chunk_MD5,
                                     upload_len, put_buffer);
                    fclose(infile);
                    fclose(infile_to_compress);
                    unlink(infile_tmp_dir);
                }
                else{   //increase count
                    int md5_ref_cnt = get(chunk_MD5);
                    fprintf(logfile, "increase md5_ref_cnt!\n");
                    md5_ref_cnt++;
                    insert(chunk_MD5, md5_ref_cnt);
                    fprintf(logfile, "md5_map_it:\n");
                    md5_map_it(md5_backup_fpath);
                }

                MD5_Init(&ctx);
                buftoread += segment_len;
                segment_len = 0;
            }

            bytes -= len;
            if (!bytes && !new_segment) {

                fprintf(logfile, "new segment!\n");
                MD5_Final(md5, &ctx);
                //update the strmap map
                //key: MD5; value: dir
                for (b = 0; b < MD5_DIGEST_LENGTH; b++) {
                    fprintf(logfile, "%02x", md5[b]);
                    sprintf(&chunk_MD5[b * 2], "%02x", md5[b]);
                }

                if(curr_chunk_ptr){ //has been initialized and extended
                    curr_chunk_ptr -> next = (struct chunk *)malloc(sizeof(struct chunk));
                    curr_chunk_ptr = curr_chunk_ptr -> next;
                }
                else{
                    curr_chunk_ptr = (struct chunk *)malloc(sizeof(struct chunk));
                    new_chunk_list = curr_chunk_ptr;
                }
                curr_chunk_ptr -> next = NULL;
                strcpy(curr_chunk_ptr -> md5, chunk_MD5);
                curr_chunk_ptr -> size = segment_len;
                //update the file size! TODO
                statbuf.st_size += segment_len;
                retval = lsetxattr(fpath_nosuffix, "user.stat", &statbuf, sizeof(struct stat), 0);
                if(retval < 0){
                    perror("update_chunk: update user.stat failed!\n");
                }

                if (!exist(chunk_MD5)) {
                    fprintf(logfile, "not exists in strmap!\n");
                    insert(chunk_MD5, 1);
                    fprintf(logfile, "md5_map_it:\n");
                    md5_map_it(md5_backup_fpath);
                    //put the segment onto the cloud, named after MD5
                    infile_to_compress = fmemopen(buftoread, segment_len, "r");
                    // infile = fopen("/mnt/ssd/comp_tmp", "w+");
                    char infile_tmp_dir[MAX_PATH_LEN];
                    fullpath("/comp_tmp", infile_tmp_dir);
                    infile = fopen(infile_tmp_dir, "w+");
                    retval = def(infile_to_compress, infile, segment_len, Z_DEFAULT_COMPRESSION);
                    if(retval != Z_OK){
                        perror("compression failed!\n");
                    }

                    fprintf(logfile, "cloud put key: %s", chunk_MD5);
                    fprintf(logfile, " ");
                    fprintf(logfile, "value: %d\n", get(chunk_MD5));
                    fseek(infile, 0, SEEK_END);
                    int upload_len = ftell(infile);
                    fseek(infile, 0, SEEK_SET);
                    cloud_put_object("cloudfs", chunk_MD5, upload_len, put_buffer);

                    fclose(infile);
                    fclose(infile_to_compress);
                    unlink(infile_tmp_dir);

                }
                else{   //increase count
                    int md5_ref_cnt = get(chunk_MD5);
                    fprintf(logfile, "increase md5_ref_cnt!\n");
                    md5_ref_cnt++;
                    insert(chunk_MD5, md5_ref_cnt);
                    fprintf(logfile, "md5_map_it:\n");
                    md5_map_it(md5_backup_fpath);
                }
                
                segment_len = 0;
                break;
            }
        }
        if (len == -1) {
            fprintf(logfile, "Failed to process the segment\n");
            break;
        }
    }
    MD5_Final(md5, &ctx);
    close(fd);
    return new_chunk_list;
}


//return the value written in this specific chunk
int write_chunk(struct file_info *curr_file, struct chunk *curr_chunk, const char *buf, size_t size, off_t offset_within_chunk){
    fprintf(logfile, "write_chunk!\n");
    FILE *chunk_file;
    int retval = -1;
    struct chunk *prev_chunk, *new_chunk_list, *new_chunk_list_tail;
    char chunk_file_fpath[MAX_PATH_LEN];   //what the fuck?
    full_cache_path(curr_chunk->md5, chunk_file_fpath);
    // fullpath(path_with_slash, chunk_file_fpath);
    chunk_file = fopen(chunk_file_fpath, "r");   //FIXME! is it the right directory?
    if(!chunk_file){    //if this chunk does not exist locally
        fprintf(logfile, "new chunk file\n");
        char outfile_tmp_dir[MAX_PATH_LEN];
        fullpath("/decom_tmp", outfile_tmp_dir);
        outfile = fopen(outfile_tmp_dir, "w+");
        chunk_file = fopen(chunk_file_fpath, "w+");

        cloud_get_object("cloudfs", curr_chunk->md5, get_buffer);
        cloud_print_error();
        fseek(outfile,0,SEEK_SET);
        retval = inf(outfile, chunk_file);
        if(retval != Z_OK){
            perror("decompression failed!\n");
        }

        fclose(outfile);
        unlink(outfile_tmp_dir);

    }

    if(offset_within_chunk + size <= curr_chunk->size){   //only current chunk need to be written
        fseek(chunk_file, offset_within_chunk, SEEK_SET);
        fwrite(buf, 1, size, chunk_file);
        retval = size;
    }
    else{
        fseek(chunk_file, offset_within_chunk, SEEK_SET);
        fwrite(buf, 1, curr_chunk->size - offset_within_chunk, chunk_file);
        retval = curr_chunk->size - offset_within_chunk;
    }

    new_chunk_list = update_chunk(curr_file, curr_chunk);

    //delete old chunk from cloud. delete old chunk, but not new chunk
    fclose(chunk_file);
    retval = unlink(chunk_file_fpath);
    assert(retval == 0);

    if(cache_map_exist(curr_chunk->md5)){
        cache_map_erase(curr_chunk->md5);
        char chunk_cache_path[MAX_PATH_LEN];
        full_cache_path(curr_chunk->md5, chunk_cache_path);
        unlink(chunk_cache_path);
        fprintf(logfile, "cur_cache_capacity: %d\n", cur_cache_capacity);
        cur_cache_capacity -= curr_chunk->size;
        fprintf(logfile, "cur_cache_capacity: %d\n", cur_cache_capacity);
        fprintf(logfile, "cache_map_it:\n");
        cache_map_it(cache_backup_fpath);
    }


    if(exist(curr_chunk->md5)){
        int cnt;
        cnt = get(curr_chunk->md5);
        cnt--;
        if(cnt == 0){
            fprintf(logfile, "delete old cache block: %s\n", chunk_file_fpath);
            cloud_delete_object("cloudfs", curr_chunk->md5);
            erase(curr_chunk->md5);
            fprintf(logfile, "md5_map_it:\n");
            md5_map_it(md5_backup_fpath);
        }
        else{
            insert(curr_chunk->md5, cnt);
            fprintf(logfile, "md5_map_it:\n");
            md5_map_it(md5_backup_fpath);
        }
    }
    if(curr_chunk != curr_file -> chunk_list){  //not head_node
        prev_chunk = curr_file -> chunk_list;
        if(!prev_chunk){
            perror("write_chunk: no prev_chunk\n");
        }
        while(prev_chunk->next != curr_chunk){
            prev_chunk = prev_chunk -> next;
        }
        prev_chunk->next = new_chunk_list;
    }
    else{   //is head_node
        curr_file -> chunk_list = new_chunk_list;
    }

    new_chunk_list_tail = new_chunk_list->next;
    if(!new_chunk_list_tail){
        new_chunk_list->next = curr_chunk->next;
    }
    else{
        while(new_chunk_list_tail -> next){
            new_chunk_list_tail = new_chunk_list_tail -> next;  //find the last one
        }
        new_chunk_list_tail -> next = curr_chunk -> next;
    }
    return retval;
}

//write the new chunk appending to the the original file
int write_chunk_new(struct file_info *curr_file, 
                    const char *buf, size_t size, 
                    off_t offset_within_chunk){
    
    char fpath[PATH_MAX];
    FILE *temp_file;
    struct chunk * new_chunk_list;

    strcpy(fpath, curr_file -> path);
    strcat(fpath, "$");

    temp_file = fopen(fpath, "w+");
    fprintf(logfile, "write_chunk_new! %s\n", fpath);

    fwrite(buf, 1, size, temp_file);

    fclose(temp_file);

    new_chunk_list = curr_file -> chunk_list;
    if(!new_chunk_list){
        perror("write_chunk_new: this proxy file has no chunk");
    }
    //fine the tail 
    while(new_chunk_list -> next){
        new_chunk_list = new_chunk_list -> next;
    }
    new_chunk_list -> next = update_chunk(curr_file, NULL);
    print_chunk_list(curr_file -> chunk_list);
    return 0;
}

void sanitize(char *path, char tmp[], int size)
{
    char *str_index = path;
    int cnt = 0;
    while (cnt < size) {
        if (*str_index == '/') {
            tmp[cnt] = '+';
        } else {
            tmp[cnt] = *str_index;
        }
        str_index++;
        cnt++;
    }
    tmp[size] = '\0';
    return;

}

void desanitize(char *path, char tmp[], int size)
{
    char *str_index = path;
    int cnt = 0;
    while (cnt < size) {
        if (*str_index == '+') {
            tmp[cnt] = '/';
        } else {
            tmp[cnt] = *str_index;
        }
        str_index++;
        cnt++;
    }
    tmp[size] = '\0';
    return;
}

static int cloudfs_error(char *error_str)
{
    int retval = -errno;

    // TODO:
    //
    // You may want to add your own logging/debugging functions for printing
    // error messages. For example:
    //
    // debug_msg("ERROR happened. %s\n", error_str, strerror(errno));
    //

    fprintf(logfile,  "CloudFS Error: %s\n", error_str);

    /* FUSE always returns -errno to caller (yes, it is negative errno!) */
    return retval;
}

/*
 * Initializes the FUSE file system (cloudfs) by checking if the mount points
 * are valid, and if all is well, it mounts the file system ready for usage.
 *
 */
void *cloudfs_init(struct fuse_conn_info *conn UNUSED)
{
    cloud_init(state_.hostname);
    fprintf(logfile, "Create bucket\n");
    cloud_create_bucket("cloudfs");
    fprintf(logfile, "Initialize string map\n");
    fprintf(logfile, "Initialize head node\n");
    int retval = -1;
    FILE * md5_backup = NULL;
    FILE * cache_backup = NULL;
    
    fullpath("/.md5_backup", md5_backup_fpath);
    fullpath("/.cache_backup", cache_backup_fpath);

    md5_backup= fopen(md5_backup_fpath, "r");
    if(!md5_backup){
        //back up file doesn't exist, create a new one
        fprintf(logfile, "md5 back up file doesn't exist, create a new one\n");
        md5_backup = fopen(md5_backup_fpath, "w+");
        //update will follow
        fclose(md5_backup);
    }
    else{
        //restore md5 map
        md5_backup = NULL;
        retval = -1;
        md5_backup = fopen(md5_backup_fpath, "r");
        if(!md5_backup){
            perror("read md5 back up file error!\n");
        }
        char buffer[5 + (MD5_DIGEST_LENGTH + 1) * 2];
        char md5_buf[(MD5_DIGEST_LENGTH + 1) * 2];
        int num;
        fprintf(logfile, "restore md5 map\n");

        while ((retval = fread(buffer, 1, (MD5_DIGEST_LENGTH) * 2 + 1 + 5, md5_backup)) > 0) {
            sscanf(buffer, "%s %4d", md5_buf, &num);
            fprintf(logfile, "%s %d\n", md5_buf, num);
            insert(md5_buf, num);
            
        }
        md5_map_it(md5_backup_fpath);
        fclose(md5_backup);
    }


    cache_backup= fopen(cache_backup_fpath, "r");
    if(!cache_backup){
        //back up file doesn't exist, create a new one
        fprintf(logfile, "cache back up file doesn't exist, create a new one\n");
        cache_backup = fopen(cache_backup_fpath, "w+");
        //update will follow
        fclose(cache_backup);
    }
    else{
        //restore cache map
        cache_backup = NULL;
        retval = -1;
        cache_backup = fopen(cache_backup_fpath, "r");
        if(!cache_backup){
            perror("read cache back up file error!\n");
        }
        char buffer[5 + (MD5_DIGEST_LENGTH + 1) * 2];
        char md5_buf[(MD5_DIGEST_LENGTH + 1) * 2];
        int num;
        fprintf(logfile, "restore cache map\n");

        while ((retval = fread(buffer, 1, (MD5_DIGEST_LENGTH) * 2 + 1 + 5, cache_backup)) > 0) {
            sscanf(buffer, "%s %4d", md5_buf, &num);
            fprintf(logfile, "%s %d\n", md5_buf, num);
            cache_map_insert(md5_buf, 1, num);
            
        }
        cache_map_it(cache_backup_fpath);
        fclose(cache_backup);
    }


  

    return NULL;
}

void cloudfs_destroy(void *data UNUSED)
{
    cloud_destroy();
}

int cloudfs_utime(const char *path, struct utimbuf *ubuf)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "utime! :%s\n",fpath);
    fprintf(logfile, "utime! :%s\n",fpath);

    int retstat = 0;
    int retval = -1;
    struct stat stat_buf;

    bool is_proxy = false;
    retval = lgetxattr(fpath, "user.is_proxy", &is_proxy, sizeof(bool));
    if((retval >= 0) && (is_proxy)){
        retval = lgetxattr(fpath, "user.stat", &stat_buf, sizeof(struct stat));
        if(retval < 0){
            fprintf(logfile, "utime: get xattrbute error!:%s\n", fpath);
        }

        stat_buf.st_atime = ubuf->actime;
        stat_buf.st_mtime = ubuf->modtime;
        lsetxattr(fpath, "user.stat", &stat_buf, sizeof(struct stat), 0);
    }
    else{
        retstat = utime(fpath, ubuf);
    }

    
    if (retstat < 0)
        retstat = cloudfs_error("cloudfs_utimens!\n");
    return retstat;
}

int cloudfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "mknod!\n");
    int retstat = 0;
    if (S_ISREG(mode)) {
        retstat = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (retstat < 0)
            retstat = cloudfs_error("cloudfs_mknod open");
        else {
            retstat = close(retstat);
            if (retstat < 0)
                retstat = cloudfs_error("cloudfs_mknod close");
        }
    } else if (S_ISFIFO(mode)) {
        retstat = mkfifo(fpath, mode);
        if (retstat < 0)
            retstat = cloudfs_error("cloudfs_mknod mkfifo");
    } else {
        retstat = mknod(fpath, mode, dev);
        if (retstat < 0)
            retstat = cloudfs_error("cloudfs_mknod mknod");
    }

    return retstat;
}

int cloudfs_getattr(const char *path, struct stat *statbuf)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "getattr! %s\n", fpath);
    int retstat = 0;
    struct stat *stat_buf = (struct stat *)malloc(sizeof(struct stat));
    bool is_proxy = false;
    int retval = lgetxattr(fpath, "user.is_proxy", &is_proxy, sizeof(bool));
    if ((retval >= 0) && (is_proxy)) {  //proxy file, get the stat from xattr
        fprintf(logfile, "is proxy\n");
        retval = lgetxattr(fpath, "user.stat", stat_buf, sizeof(struct stat));
        fprintf(logfile, "getattr user.stat file: %d\n", retval);
        fprintf(logfile, "fpath: %s, size %d\n", fpath, (int)stat_buf->st_size);
        fprintf(logfile, "getattr user.stat file: %d\n", retval);
        fprintf(logfile, "fpath: %s, size %d\n", fpath, (int)stat_buf->st_size);
        memcpy(statbuf, stat_buf, sizeof(struct stat));
        free(stat_buf);
    } else {                    //regular file, regular way
        retstat = lstat(fpath, statbuf);
        fprintf(logfile, "lstat:%d\n", retstat);
    }
    if (retstat < 0)
        retstat = cloudfs_error("cloudfs_getattr!\n");
    return retstat;
}

int
cloudfs_getxattr(const char *path, const char *name, char *value,
                 size_t size)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "getxattr!\n");
    int retstat = 0;
    retstat = lgetxattr(fpath, name, value, size);
    if (retstat < 0) {
        retstat = cloudfs_error("cloudfs_getxattr!\n");
    }
    return retstat;
}

int
cloudfs_setxattr(const char *path, const char *name, const char *value,
                 size_t size, int flags)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "setxattr!\n");
    int retstat = 0;
    retstat = lsetxattr(fpath, name, value, size, flags);
    if (retstat < 0)
        retstat = cloudfs_error("setxattr!\n");
    return retstat;
}

int cloudfs_mkdir(const char *path, mode_t mode)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "mkdir!\n");
    int retstat = 0;
    retstat = mkdir(fpath, mode);
    if (retstat < 0)
        retstat = cloudfs_error("cloudfs_mkdir!\n");
    return retstat;
}

int cloudfs_rmdir(const char *path)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "rmdir!\n");
    int retstat = 0;
    retstat = rmdir(fpath);
    if (retstat < 0)
        retstat = cloudfs_error("cloudfs_rmdir!\n");
    return retstat;
}

int cloudfs_unlink(const char *path)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile,  "unlink!\n");
    int retstat = 0;
    int md5_ref_cnt = 0;
    bool is_proxy = false;
    struct file_info *tmp;
    struct file_info *prev;
    struct chunk *chunk_tmp;
    int chunk_size;
    int retval = lgetxattr(fpath, "user.is_proxy", &is_proxy, sizeof(bool));
    if (retval >= 0 && is_proxy) {//proxy, delete the file on the cloud as well
        //decrease the md5 ref cnt, if 0, delete from map and cloud
        tmp = head_node;
        prev = NULL;
        while(tmp != NULL && (strcmp(fpath, tmp->path))){
            prev = tmp;
            tmp = tmp->next;
        }
        if(tmp == NULL){
            fprintf(logfile, "unlink: can't find file_info node!\n");
            //use proxy file to clean the cloud
            int fd = open(fpath, O_RDWR);
            char buffer[5 + (MD5_DIGEST_LENGTH + 1) * 2];
            char md5_buf[(MD5_DIGEST_LENGTH + 1) * 2];
            chunk_size = 0;
            while ((retval = read(fd, buffer, (MD5_DIGEST_LENGTH) * 2 + 1 + 5)) > 0) {

                sscanf(buffer, "%4d %s", &chunk_size, md5_buf);
                fprintf(logfile, "unlink: get components: %s\n", md5_buf);
                if(!exist(md5_buf)){
                    fprintf(logfile, "unlink: why can't find this chunk?\n");
                }
                md5_ref_cnt = get(md5_buf);
                md5_ref_cnt--;
                if(md5_ref_cnt > 0){   //set it back
                    insert(md5_buf, md5_ref_cnt);
                    fprintf(logfile, "md5_map_it:\n");
                    md5_map_it(md5_backup_fpath);
                }
                else{
                    //delete it from cloud and cache
                    erase(md5_buf);
                    fprintf(logfile, "erase: %s \n",md5_buf);
                    fprintf(logfile, "md5_map_it:\n");
                    md5_map_it(md5_backup_fpath);
                    cloud_delete_object("cloudfs", md5_buf);
                    cloud_print_error();

                    fprintf(logfile, "clear .cache: %s\n", md5_buf);
                    // cache_map_erase(chunk_tmp->md5);
                    char chunk_file_fpath[MAX_PATH_LEN];
                    full_cache_path(md5_buf, chunk_file_fpath);
                    retval = unlink(chunk_file_fpath);
                    if(retval){
                        fprintf(logfile, "unlink %s of %s fail!\n", chunk_file_fpath, fpath);
                    }
                    if(cache_map_exist(md5_buf)){
                        cache_map_erase(md5_buf);

                        chunk_file_fpath[MAX_PATH_LEN] = memset(chunk_file_fpath, 0, MAX_PATH_LEN); //nonsensical?
                        full_cache_path(md5_buf, chunk_file_fpath);
                        retval = unlink(chunk_file_fpath);
                        if(retval){
                            fprintf(logfile, "unlink %s of %s fail!\n", chunk_file_fpath, fpath);
                        }

                        cur_cache_capacity -= chunk_size;
                        fprintf(logfile, "valid clear .cache: %s\n", md5_buf);
                        fprintf(logfile, "cache_map_it:\n");
                        cache_map_it(cache_backup_fpath);
                        fprintf(logfile, "cur_cache_capacity: %d\n", cur_cache_capacity);
                    }
                }
            }
            fprintf(logfile, "md5_map_it:\n");
            md5_map_it(md5_backup_fpath);
            fprintf(logfile, "cache_map_it:\n");
            int chunk_num;
            chunk_num = cache_map_it(cache_backup_fpath);
            fprintf(logfile, "cache usage: %d/%d\n", cur_cache_capacity, state_.cache_size);
            fprintf(logfile, "cur_cache_capacity: %d, chunk_num: %d\n", cur_cache_capacity, chunk_num);
            close(fd);
        }
        else{
            chunk_tmp = tmp->chunk_list;
            while(chunk_tmp != NULL){
                md5_ref_cnt = get(chunk_tmp->md5);
                md5_ref_cnt--;
                if(md5_ref_cnt > 0){   //set it back
                    insert(chunk_tmp->md5, md5_ref_cnt);
                    fprintf(logfile, "md5_map_it:\n");
                    md5_map_it(md5_backup_fpath);
                }
                else{   //delete from map and cloud
                    erase(chunk_tmp->md5);
                    fprintf(logfile, "erase: %s \n",chunk_tmp->md5);
                    fprintf(logfile, "md5_map_it:\n");
                    md5_map_it(md5_backup_fpath);
                    cloud_delete_object("cloudfs", chunk_tmp->md5);
                    cloud_print_error();
                    if(cache_map_exist(chunk_tmp->md5)){
                        fprintf(logfile, "clear .cache: %s\n", chunk_tmp->md5);
                        cache_map_erase(chunk_tmp->md5);
                        char chunk_file_fpath[MAX_PATH_LEN];
                        full_cache_path(chunk_tmp->md5, chunk_file_fpath);
                        retval = unlink(chunk_file_fpath);
                        if(retval){
                            fprintf(logfile, "unlink %s of %s fail!\n", chunk_file_fpath, fpath);
                        }
                        assert(retval == 0);
                        chunk_size = chunk_tmp->size;
                        fprintf(logfile, "cur_cache_capacity: %d\n", cur_cache_capacity);
                        cur_cache_capacity -= chunk_size;
                        fprintf(logfile, "cur_cache_capacity: %d\n", cur_cache_capacity);
                        fprintf(logfile, "cache_map_it:\n");
                        cache_map_it(cache_backup_fpath);
                    }
                }
                chunk_tmp = chunk_tmp->next;
            }

            fprintf(logfile, "md5_map_it:\n");
            md5_map_it(md5_backup_fpath);
            fprintf(logfile, "cache_map_it:\n");
            int chunk_num;
            chunk_num = cache_map_it(cache_backup_fpath);
            fprintf(logfile, "cache usage: %d/%d\n", cur_cache_capacity, state_.cache_size);
            fprintf(logfile, "cur_cache_capacity: %d, chunk num:%d\n", cur_cache_capacity, chunk_num);
            //delete file_info node

            if(tmp == head_node){
                //head_node
                head_node = head_node->next;
            }
            else{
                prev->next = tmp->next;
            }

        }
    }

    retstat = unlink(fpath);

    if (retstat < 0)
        retstat = cloudfs_error("cloudfs_unlink!\n");
    return retstat;
}

int cloudfs_chmod(const char *path, mode_t mode)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "chmod!\n");
    int retstat = 0;

    int retval = -1;
    struct stat stat_buf;

    bool is_proxy = false;
    retval = lgetxattr(fpath, "user.is_proxy", &is_proxy, sizeof(bool));
    if((retval >= 0) && (is_proxy)){
        retval = lgetxattr(fpath, "user.stat", &stat_buf, sizeof(struct stat));
        if(retval < 0){
            fprintf(logfile, "chmod: get xattrbute error!:%s\n", fpath);
        }
        retstat = chmod(fpath, stat_buf.st_mode);
    }
    else{
        retstat = chmod(fpath, mode);
    }

    if (retstat < 0)
        retstat = cloudfs_error("cloudfs_chmod!\n");
    return retstat;
}

int cloudfs_truncate(const char *path, off_t newsize)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "truncate!\n");
    int retstat = 0;
    retstat = truncate(fpath, newsize);
    if (retstat < 0)
        cloudfs_error("truncate!\n");
    return retstat;
}

int cloudfs_release(const char *path, struct fuse_file_info *ffi)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "release! %s\n", fpath);
    int retstat = 0;

    int open_cnt = -1;

    int fd = -1;
    int retval = -1;
    bool is_proxy = false;
    retval = lgetxattr(fpath, "user.is_proxy", &is_proxy, sizeof(bool));
    if (retval > 0 && is_proxy) {
        retval = lgetxattr(fpath,"user.open_cnt", &open_cnt, sizeof(int));
        if(retval < 0){
            perror("release, is_proxy, get open_cnt fail\n");
        }
        open_cnt--;
        retval = lsetxattr(fpath, "user.open_cnt", &open_cnt, sizeof(int), 0);
        if(retval < 0){
            perror("release: not proxy, set xattr open cnt fail\n");
        }
        //if return from read(), unlink the dummy file, return
        int rw = -1;
        retval = lgetxattr(fpath, "user.rw", &rw, sizeof(int));
        if (retval > 0 && rw == READ) {
            retstat = close(ffi->fh);
            return retstat;
        }

        if (retval > 0 && rw == WRITE) {
            //update md5 list on the disk
            //truncate first
            fprintf(logfile, "truncate\n");
            fd = open(fpath, O_RDWR | O_TRUNC);
            close(fd);
            FILE *f_md5;
            f_md5 = fopen(fpath, "w");
            struct file_info *curr_file_info = head_node;
            while(curr_file_info && strcmp(fpath, curr_file_info->path)){
                curr_file_info = curr_file_info -> next;
            }
            if(!curr_file_info){
                fprintf(logfile, "WTF?????\n");
            }
            struct chunk *new_chunk_list = curr_file_info -> chunk_list;
            while(new_chunk_list){
                retval = fprintf(f_md5, "%04d %s\n", new_chunk_list->size, new_chunk_list->md5);
                if(retval < 0){
                    perror("release from write: update proxy file fail");
                }
                new_chunk_list = new_chunk_list->next;
            }
            fclose(f_md5);

            //delete temp file ended with $
            strcat(fpath, "$");
            unlink(fpath);

            retstat = close(ffi->fh);

            return retstat;
        }
    }

    retval = lgetxattr(fpath, "user.open_cnt", &open_cnt, sizeof(int));
    fprintf(logfile, "getxattr open_cnt retval:%d,open_cnt:%d, path: %s\n", retval,
           open_cnt, fpath);
    if (retval < 0) {
        retstat = close(ffi->fh);

        return retstat;
    }
    open_cnt--;
    retval = lsetxattr(fpath, "user.open_cnt", &open_cnt, sizeof(int), 0);
    if(retval < 0){
        perror("release: not proxy, set xattr open cnt fail\n");
    }
    fprintf(logfile, "read open_cnt successful!\n");
    if (open_cnt == 0) {        //not open in other place
        fprintf(logfile, "open_cnt == 0!\n");
        //check if it's a big file or small file
        struct stat statbuf;
        //get the size of the file
        fprintf(logfile, "%s\n", fpath);
        int ret = lstat(fpath, &statbuf);
        size_t file_size = 0;
        fprintf(logfile, "lstat:%d, errno:%d\n", ret, errno);
        if (ret == 0) {         //without error
            file_size = statbuf.st_size;
        }
        fprintf(logfile, "file_size: %d, state_.threshold:%d\n", file_size,
               state_.threshold);
        if (file_size > (unsigned int) state_.threshold) {      //big file
            fprintf(logfile, "big file!\n");
            //upload to cloud
            //segment, add all the MD5 of its chunks into table
            rabinpoly_t *rp = rabin_init(window_size, avg_seg_size, min_seg_size, max_seg_size);
            if (!rp) {
                fprintf(logfile, "Failed to init rabinhash algorithm\n");
            }

            MD5_CTX ctx;
            unsigned char md5[MD5_DIGEST_LENGTH];
            int i;
            for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
                md5[i] = '\0';
            }
            int new_segment = 0;
            int len, segment_len = 0, b;
            char buf[8192];
            int bytes;
            MD5_Init(&ctx);
            fd = open(fpath, O_RDWR);
            char chunk_MD5[MD5_DIGEST_LENGTH * 2 + 2];
            char *chunks[file_size / min_seg_size];
            int chunk_size[file_size / min_seg_size];
            int chunk_num = 0;
            FILE *infile_to_compress = NULL;
            while ((bytes = read(fd, buf, sizeof buf)) > 0) {
                char *buftoread = (char *) &buf[0];
                while ((len =
                        rabin_segment_next(rp,
                                           buftoread +
                                           segment_len, bytes,
                                           &new_segment)) > 0) {
                    MD5_Update(&ctx, buftoread, len);
                    segment_len += len;

                    if (new_segment) {
                        fprintf(logfile, "new segment!\n");
                        MD5_Final(md5, &ctx);
                        //update the strmap map
                        //key: MD5; value: dir
                        for (b = 0; b < MD5_DIGEST_LENGTH; b++) {
                            fprintf(logfile, "%02x", md5[b]);
                            sprintf(&chunk_MD5[b * 2], "%02x", md5[b]);
                        }

                        chunks[chunk_num] = (char *) malloc((MD5_DIGEST_LENGTH * 2 + 2) * sizeof(char));
                        strcpy(chunks[chunk_num], chunk_MD5);
                        chunk_size[chunk_num] = segment_len;
                        chunk_num++;
                        if (!exist(chunk_MD5)) {
                            fprintf(logfile, "not exists in strmap!\n");
                            insert(chunk_MD5, 1);
                            fprintf(logfile, "md5_map_it:\n");
                            md5_map_it(md5_backup_fpath);
                            //put the segment onto the cloud, named after MD5
                            infile_to_compress = fmemopen(buftoread, segment_len, "r");
                            char infile_tmp_dir[MAX_PATH_LEN];
                            fullpath("/comp_tmp", infile_tmp_dir);
                            infile = fopen(infile_tmp_dir, "w+");
                            retval = def(infile_to_compress, infile, segment_len, Z_DEFAULT_COMPRESSION);
                            if(retval != Z_OK){
                                perror("compression failed!\n");
                                fprintf(logfile, "compression failed!\n");
                            }

                            fprintf(logfile, "key: %s", chunk_MD5);
                            fprintf(logfile, " ");
                            fprintf(logfile, "value: %d\n", get(chunk_MD5));

                            int upload_len;
                            fseek(infile, 0, SEEK_END);
                            upload_len = ftell(infile);
                            fseek(infile, 0, SEEK_SET);

                            cloud_put_object("cloudfs",
                                             chunk_MD5,
                                             upload_len, put_buffer);
                            cloud_print_error();
                            fclose(infile);
                            fclose(infile_to_compress);
                            unlink(infile_tmp_dir);
                        }
                        else{   //increase count
                            int md5_ref_cnt = get(chunk_MD5);
                            fprintf(logfile, "increase md5_ref_cnt!\n");
                            md5_ref_cnt++;
                            insert(chunk_MD5, md5_ref_cnt);
                            fprintf(logfile, "md5_map_it:\n");
                            md5_map_it(md5_backup_fpath);
                        }

                        MD5_Init(&ctx);
                        buftoread += segment_len;
                        segment_len = 0;
                    }

                    bytes -= len;
                    if (!bytes && !new_segment) {

                        fprintf(logfile, "new segment!\n");
                        MD5_Final(md5, &ctx);
                        //update the strmap map
                        //key: MD5; value: dir
                        for (b = 0; b < MD5_DIGEST_LENGTH; b++) {
                            fprintf(logfile, "%02x", md5[b]);
                            sprintf(&chunk_MD5[b * 2], "%02x", md5[b]);
                        }

                        chunks[chunk_num] = (char *) malloc((MD5_DIGEST_LENGTH * 2 + 2) * sizeof(char));
                        strcpy(chunks[chunk_num], chunk_MD5);
                        chunk_size[chunk_num] = segment_len;
                        chunk_num++;
                        if (!exist(chunk_MD5)) {
                            fprintf(logfile, "not exists in strmap!\n");
                            insert(chunk_MD5, 1);
                            fprintf(logfile, "md5_map_it:\n");
                            md5_map_it(md5_backup_fpath);
                            //put the segment onto the cloud, named after MD5
                            infile_to_compress = fmemopen(buftoread, segment_len, "r");
                            char infile_tmp_dir[MAX_PATH_LEN];
                            fullpath("/comp_tmp", infile_tmp_dir);
                            infile = fopen(infile_tmp_dir, "w+");
                            retval = def(infile_to_compress, infile, segment_len, Z_DEFAULT_COMPRESSION);
                            if(retval != Z_OK){
                                perror("compression failed!\n");
                            }


                            fprintf(logfile, "cloud put key: %s", chunk_MD5);
                            fprintf(logfile, " ");
                            fprintf(logfile, "value: %d\n", get(chunk_MD5));

                            int upload_len;
                            fseek(infile, 0, SEEK_END);
                            upload_len = ftell(infile);
                            fseek(infile, 0, SEEK_SET);

                            cloud_put_object("cloudfs", chunk_MD5, upload_len, put_buffer);
                            cloud_print_error();
                            fclose(infile);
                            fclose(infile_to_compress);
                            unlink(infile_tmp_dir);
                        }
                        else{   //increase count
                            int md5_ref_cnt = get(chunk_MD5);
                            fprintf(logfile, "increase md5_ref_cnt!\n");
                            md5_ref_cnt++;
                            insert(chunk_MD5, md5_ref_cnt);
                            fprintf(logfile, "md5_map_it:\n");
                            md5_map_it(md5_backup_fpath);
                        }
                        
                        segment_len = 0;
                        break;
                    }
                }
                if (len == -1) {
                    fprintf(logfile, "Failed to process the segment\n");
                    break;
                }
            }
            MD5_Final(md5, &ctx);

            struct stat stat_buf;
            lstat(fpath, &stat_buf);
            //set the extending attribute is_proxy
            is_proxy = true;
            lsetxattr(fpath, "user.is_proxy", &is_proxy, sizeof(bool), 0);
            //save the stat in xattr
            lsetxattr(fpath, "user.stat", &statbuf,
                      sizeof(struct stat), 0);
            //save MD5 segment information
            //make the local replica proxy, that is, clear the file

            close(fd);
            fprintf(logfile, "truncate\n");
            fd = open(fpath, O_RDWR | O_TRUNC);
            close(fd);
            FILE *f_md5;
            f_md5 = fopen(fpath, "w");

            fprintf(logfile, "retval: %d\n", retval);
            int j;
            for (j = 0; j < chunk_num; j++) {
                fprintf(logfile, "md5 in release: %s\n", chunks[j]);
                retval = fprintf(f_md5, "%04d %s\n", chunk_size[j], chunks[j]);
                if (retval < 0) {
                    perror("write:");
                }
            }
            for (j = 0; j < chunk_num; j++) {
                free(chunks[j]);
            }
            fclose(f_md5);
        }
    }
    retstat = close(ffi->fh);

    return retstat;
}

int cloudfs_open(const char *path, struct fuse_file_info *ffi)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "open! %s\n", fpath);
    int retstat = 0;
    int fd;
    fd = open (fpath, O_RDWR);
    //set extend attribute is_proxy and open_cnt
    bool is_proxy = false;
    int retval =
        lgetxattr(fpath, "user.is_proxy", &is_proxy, sizeof(bool));
    if (retval < 0) {           //no is_proxy attribute, set to default
        is_proxy = false;
        int ret = lsetxattr(fpath, "user.is_proxy", &is_proxy,
                            sizeof(bool), 0);
        fprintf(logfile, "setxattr is_proxy : %d, is_proxy:%d, path: %s\n", ret, is_proxy, fpath);
    } else {                    //valid is_proxy attribute, check if it's a proxy file

        if (is_proxy) {         //it's a proxy file, retrieve it from the cloud
            fprintf(logfile, "try to read?\n");
            //populate the chunk list, so read can know which chunk to get.
            //FIXME check if double opened
            struct file_info *curr = NULL;
            if (head_node != NULL) {
                curr = head_node;
                while (curr && strcmp(curr->path, fpath)) {
                    curr = curr->next;
                }
            }
            //not populated before
            if (!curr) {
                curr = (struct file_info *)malloc(sizeof(struct file_info));
                curr->next = head_node;
                head_node = curr;
                strcpy(curr->path, fpath);
                curr->chunk_list = NULL;

                struct chunk **chunk_ptr;
                chunk_ptr = &(curr->chunk_list);

                fprintf(logfile, "populate the chunk_list\n");
                char buffer[5 + (MD5_DIGEST_LENGTH + 1) * 2];
                while ((retval = read(fd, buffer, (MD5_DIGEST_LENGTH) * 2 + 1 + 5)) > 0) {
                    *chunk_ptr = (struct chunk *)malloc(sizeof(struct chunk));
                    (*chunk_ptr)->next = NULL;

                    sscanf(buffer, "%4d %s", &((*chunk_ptr)->size),
                           (*chunk_ptr)->md5);
                    fprintf(logfile, "components: %s\n", (*chunk_ptr)->md5);
                    chunk_ptr = &((*chunk_ptr)->next);
                }
            }
            else{
                fprintf(logfile, "file_info populated already!\n");
            }
            if(retval < 0){
                perror("not populate chunk list:");
            }
            fprintf(logfile, "bad retval? %d\n", retval);
        } else {                //it's not a proxy file, open it locally
            //already opened
            fprintf(logfile, "not proxy\n");
            //give read the real fd:
        }
    }
    int open_cnt = 0;
    retval = lgetxattr(fpath, "user.open_cnt", &open_cnt, sizeof(int));
    if (retval < 0) {           //no open_cnt attribute, set to default
        open_cnt = 1;
        int setxa_ret = lsetxattr(fpath, "user.open_cnt", &open_cnt,
                                  sizeof(int), 0);
        fprintf(logfile, "setxattr open_cnt complete:%d, path: %s\n", setxa_ret, fpath);
    } else {                    //valid open_cnt attribute, increment by 1
        open_cnt++;
        int setxa_ret = lsetxattr(fpath, "user.open_cnt", &open_cnt, sizeof(int), 0);
        fprintf(logfile, "setxattr open_cnt complete:%d, path: %s\n", setxa_ret, fpath);
    }

    if (fd < 0)
        retstat = cloudfs_error("cloudfs_open!\n");
    
    ffi->fh = fd;
    return retstat;
}

int
cloudfs_read(const char *path, char *buf, size_t nbyte, off_t offset, struct fuse_file_info *ffi)
{
    char fpath[PATH_MAX];
    char *chunk_to_evict;
    struct file_info *cur;
    size_t acc_segment_size = 0;
    struct chunk *chunk_list_index;
    fullpath(path, fpath);
    fprintf(logfile,  "read! %s\n", fpath);
    fprintf(logfile, "read %s\n", fpath);
    int rw = READ;
    lsetxattr(fpath, "user.rw", &rw, sizeof(int), 0);
    bool is_proxy = false;
    int retval =
        lgetxattr(fpath, "user.is_proxy", &is_proxy, sizeof(bool));
    if (retval < 0) {
        perror("in read, is_proxy not set!\n");
    }
    if (!is_proxy) {
        lseek(ffi->fh, offset, SEEK_SET);
        int retstat = read(ffi->fh, buf, nbyte);
        if (retstat < 0){
            fprintf(logfile, "**********************************%s", strerror(errno));
            retstat = cloudfs_error("cloudfs_read~\n");
        }
        return retstat;
    } 
    else {
        //get the correct chunks from the cloud and fill into the dummy file
        cur = head_node;
        while (strcmp(cur->path, fpath)) {
            cur = cur->next;
        }
        
        chunk_list_index = cur->chunk_list;

        while (1) {
            if (acc_segment_size + chunk_list_index->size >= offset) {      //DANGEROUS!!!
                break;
            }
            acc_segment_size += chunk_list_index->size;
            chunk_list_index = chunk_list_index->next;
        }

        //now index point to the first chunk we have to fetch
        char chunk_file_fpath[MAX_PATH_LEN];
        // full_cache_path(chunk_list_index->md5, chunk_file_fpath);
        fprintf(logfile, "chunk_file_fpath: %s\n", chunk_file_fpath);
        char evict_fpath[MAX_PATH_LEN];
        int size_read = 0;
        int offset_within_chunk = 0;
        FILE *chunk_to_read = NULL;
        char outfile_tmp_dir[MAX_PATH_LEN];
        fullpath("/decom_tmp", outfile_tmp_dir);
        while (acc_segment_size < (offset + nbyte))     //FIXME!
        {
            memset(chunk_file_fpath,0,MAX_PATH_LEN);
            full_cache_path(chunk_list_index->md5, chunk_file_fpath);
            if(!cache_map_exist(chunk_list_index->md5)){
                while((cur_cache_capacity + chunk_list_index->size) > (state_.cache_size - 32)){
                    fprintf(logfile, "cur_cache_capacity: %d, size: %d, threshold: %d\n", cur_cache_capacity, chunk_list_index->size, state_.cache_size - 32);
                    chunk_to_evict = eviction();
                    fprintf(logfile, "chunk_to_evict: %s\n", chunk_to_evict);
                    fprintf(logfile, "cache usage: %d/%d\n", cur_cache_capacity, state_.cache_size);
                    fprintf(logfile, "cur_cache_capacity: %d\n", cur_cache_capacity);
                    full_cache_path(chunk_to_evict, evict_fpath);

                    retval = unlink(evict_fpath);
                    assert(retval == 0);
                }
                /*cache_map_insert(chunk_list_index->md5, 1, chunk_list_index->size); //the size should be the compressed size! TODO!

                fprintf(logfile, "cur_cache_capacity: %d\n", cur_cache_capacity);
                cur_cache_capacity += chunk_list_index->size;
                fprintf(logfile, "cur_cache_capacity: %d\n", cur_cache_capacity);
                fprintf(logfile, "cache_map_it:\n");
                cache_map_it(cache_backup_fpath);*/


                // char outfile_tmp_dir[MAX_PATH_LEN];
                // fullpath("/decom_tmp", outfile_tmp_dir);
                // outfile = fopen(outfile_tmp_dir, "w+");
                outfile = fopen(chunk_file_fpath, "w+");    //.cache/md5

                fprintf(logfile, "open file dir: %s\n", chunk_file_fpath);
                chunk_to_read = fopen(outfile_tmp_dir, "w+");   //tmp file to read

                if(!chunk_to_read){
                    fprintf(logfile, "fopen failed! %d \n", (int)chunk_to_read);
                    fprintf(logfile, "Failed to open %s\n", chunk_file_fpath, strerror(errno));
                    fprintf(logfile, "errno: %d\n", errno);
                    perror("error");
                }   

                cloud_get_object("cloudfs", chunk_list_index->md5, get_buffer);
                fprintf(logfile, "read from cloud %s\n", chunk_list_index->md5);
                cloud_print_error();
                fseek(outfile, 0, SEEK_END);
                int size = ftell(outfile);
                fseek(outfile,0,SEEK_SET);

                cache_map_insert(chunk_list_index->md5, 0, size); //the size should be the compressed size! TODO!
                update_LRU(chunk_list_index->md5);

                fprintf(logfile, "cur_cache_capacity: %d\n", cur_cache_capacity);
                cur_cache_capacity += size;
                fprintf(logfile, "cur_cache_capacity: %d\n", cur_cache_capacity);
                fprintf(logfile, "cache_map_it:\n");
                cache_map_it(cache_backup_fpath);





                fprintf(logfile, "before inflation\n");
                retval = inf(outfile, chunk_to_read);
                fprintf(logfile, "after inflation, retval: %d\n", retval);
                if(retval != Z_OK){
                    perror("decompression failed!\n");
                }



                fprintf(logfile, "before fclose\n");
                fclose(outfile);
                // fprintf(logfile, "start unlink\n");
                // unlink(outfile_tmp_dir);
                // fprintf(logfile, "after unlink\n");
            }
            else{
                // chunk_to_read = fopen(chunk_file_fpath, "r");
                FILE *compressed_chunk = NULL;
                compressed_chunk = fopen(chunk_file_fpath, "r");
                if(!compressed_chunk){
                    fprintf(logfile, "errno: %d\n", errno);
                }
                // char outfile_tmp_dir[MAX_PATH_LEN];
                // fullpath("/decom_tmp", outfile_tmp_dir);
                chunk_to_read = fopen(outfile_tmp_dir, "w+");
                // fseek(compressed_chunk,0,SEEK_SET);
                retval = inf(compressed_chunk, chunk_to_read);
                if(retval != Z_OK){
                    fprintf(logfile, "decompression failed!\n");
                }
                
                fprintf(logfile, "read locally\n");
                update_LRU(chunk_list_index->md5);
                fclose(compressed_chunk);
            }
            
            if(!chunk_to_read){
                perror("chunk_to_read: ");
            }

            offset_within_chunk = offset + size_read - acc_segment_size;
            fprintf(logfile, "offset within chunk: %d\n", offset_within_chunk);
            fseek(chunk_to_read, offset_within_chunk, SEEK_SET);
            assert(size_read <= nbyte);
            if(nbyte - size_read >= chunk_list_index->size - offset_within_chunk){
                fprintf(logfile, "bytes to read: %d\n", chunk_list_index->size - offset_within_chunk);
                retval = fread(buf + size_read, 1, chunk_list_index->size - offset_within_chunk, chunk_to_read);
            }
            else{
                fprintf(logfile, "bytes to read: %d\n", nbyte - size_read);
                retval = fread(buf + size_read, 1, nbyte - size_read, chunk_to_read);
            }

            if(retval < 0){
                perror("chunk read: ");
            }
            fprintf(logfile, "bytes read in this chunk: %d\n", retval);
            size_read += retval;


            if(chunk_to_read){
                fclose(chunk_to_read);
                chunk_to_read = NULL;
                fprintf(logfile, "start unlink\n");
                unlink(outfile_tmp_dir);
                fprintf(logfile, "after unlink\n");
            }
            
            if (!chunk_list_index->next) {
                break;
            }
            acc_segment_size += chunk_list_index->size;
            chunk_list_index = chunk_list_index->next;
            
        }

        int retstat = nbyte;
        if (retstat < 0){
            fprintf(logfile, "*************************************%s", strerror(errno));
        }

        fprintf(logfile, "md5_map_it:\n");
        md5_map_it(md5_backup_fpath);
        fprintf(logfile, "cache_map_it:\n");
        int chunk_num;
        chunk_num = cache_map_it(cache_backup_fpath);
        fprintf(logfile, "cache usage: %d/%d\n", cur_cache_capacity, state_.cache_size);
        fprintf(logfile, "cur_cache_capacity: %d, chunk num:%d\n", cur_cache_capacity, chunk_num);
        return retstat;
    }
}

int
cloudfs_write(const char *path, const char *buf, size_t size,
              off_t offset, struct fuse_file_info *ffi)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "write! %s\n", fpath);
    int rw = WRITE;
    lsetxattr(fpath, "user.rw", &rw, sizeof(int), 0);
    int retstat = 0;    //-1?
    int acc_segment_size, size_written, offset_within_chunk;
    struct chunk *curr_chunk;
    struct file_info *curr_file_info;
    //use lseek to find the offset, seemed of so far;
    size_written = 0;
    acc_segment_size = 0;
    //find the curr_file_info
    curr_file_info = head_node;

    bool is_proxy = false;
    int retval =
        lgetxattr(fpath, "user.is_proxy", &is_proxy, sizeof(bool));
    if (retval < 0) {
        perror("in write, is_proxy not set!\n");
    }
    //regular local file:
    if(!is_proxy){
        lseek(ffi->fh, offset, SEEK_SET);
        fprintf(logfile, "write: regular file write!\n");
        retstat = write(ffi->fh, buf, size); 
        if(retstat < 0){
            retstat = cloudfs_error("cloudfs_write: error!\n");
        }
        return retstat;
    }
    //segmented chunk file:

    if(!curr_file_info){
        perror("cloudfs_write: the head_node can't be NULL !\n");
    }
    while(curr_file_info && strcmp(fpath, curr_file_info->path)){
        curr_file_info = curr_file_info -> next;
    }
    curr_chunk = curr_file_info -> chunk_list;
    acc_segment_size = 0;

    while(curr_chunk && (acc_segment_size + curr_chunk -> size <= offset)){  
        //this chunk is not the start chunk
        acc_segment_size += curr_chunk -> size;
        curr_chunk = curr_chunk -> next;
    }

    while(curr_chunk && (size_written < size)){
        offset_within_chunk = offset +size_written - acc_segment_size;
        size_written += write_chunk(curr_file_info, 
                                    curr_chunk, 
                                    buf + size_written, 
                                    size - size_written, 
                                    offset_within_chunk);
        acc_segment_size += curr_chunk -> size;
        curr_chunk = curr_chunk -> next;
        if(!curr_chunk){
            break;
        }
    }
    //reach the bottom of original file, still need to write
    //append new segment
    if(!curr_chunk && (size_written < size)){
        write_chunk_new(curr_file_info, 
                        buf + size_written, 
                        size - size_written, 
                        offset + size_written - acc_segment_size);

    }

    if (retstat < 0)
        retstat = cloudfs_error("cloudfs_write!\n");
    return size;

}

int cloudfs_readlink(const char *path, char *rlink, size_t size)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "readlink!\n");
    int retstat = 0;
    retstat = readlink(fpath, rlink, size - 1);
    if (retstat < 0) {
        retstat = cloudfs_error("readlink!\n");
    } else {
        rlink[retstat] = '\0';
        retstat = 0;
    }
    return retstat;
}

int cloudfs_opendir(const char *path, struct fuse_file_info *ffi)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "cloudfs_opendir! %s\n", fpath);
    DIR *dp;
    int retstat = 0;
    dp = opendir(fpath);
    if (dp == NULL)
        retstat = cloudfs_error("cloudfs_opendir\n");
    ffi->fh = (intptr_t) dp;

    return retstat;
}

int
cloudfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                off_t offset, struct fuse_file_info *ffi)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "cloudfs_readdir %s\n", fpath);
    int retstat = 0;
    DIR *dp;
    struct dirent *de;

    dp = (DIR *) (uintptr_t) ffi->fh;
    de = readdir(dp);
    if (de == 0) {
        retstat = cloudfs_error("readdir error!\n");
        return retstat;
    }
    do {
        if (filler(buf, de->d_name, NULL, 0) != 0) {
            return -ENOMEM;
        }

    }
    while ((de = readdir(dp)) != NULL);
    return retstat;
}

int cloudfs_access(const char *path, int mask)
{
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "access!\n");
    int retstat = 0;
    retstat = access(fpath, mask);
    if (retstat < 0)
        return cloudfs_error("cloudfs_access!\n");
    return retstat;
}

int cloudfs_chown(const char *path, uid_t uid, gid_t gid)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "chown!: %s\n", fpath);


    int retval = -1;
    struct stat stat_buf;

    bool is_proxy = false;
    retval = lgetxattr(fpath, "user.is_proxy", &is_proxy, sizeof(bool));
    if((retval >= 0) && (is_proxy)){
        retval = lgetxattr(fpath, "user.stat", &stat_buf, sizeof(struct stat));
        if(retval < 0){
            fprintf(logfile, "chown: get xattrbute error!:%s\n", fpath);
        }
        retstat = chown(fpath, stat_buf.st_uid, stat_buf.st_gid);
    }
    else{
        retstat = chown(fpath, uid, gid);
    }
    
    if (retstat < 0) {
        retstat = cloudfs_error("cloudfs_chown!\n");
    }
    return retstat;

}

int cloudfs_create(const char *path, mode_t mode,
                   struct fuse_file_info *ffi)
{

    int retstat = 0;
    int retval = -1;
    char fpath[PATH_MAX];
    fullpath(path, fpath);
    fprintf(logfile, "create! %s\n", fpath);
    int fd;

    fd = creat(fpath, mode);

    int open_cnt = 0;
    retval = lgetxattr(fpath, "user.open_cnt", &open_cnt, sizeof(int));
    if (retval < 0) {           //no open_cnt attribute, set to default
        fprintf(logfile, "create: set open_cnt\n");
        open_cnt = 1;
        int setxa_ret = lsetxattr(fpath, "user.open_cnt", &open_cnt,
                                  sizeof(int), 0);
        fprintf(logfile, "setxattr open_cnt complete:%d, path: %s\n", setxa_ret, fpath);
    } else {                    //valid open_cnt attribute, increment by 1
        open_cnt++;
        fprintf(logfile, "create: this cannot happen\n");
        int setxa_ret = lsetxattr(fpath, "user.open_cnt", &open_cnt, sizeof(int), 0);
        fprintf(logfile, "setxattr open_cnt complete:%d, path: %s\n", setxa_ret, fpath);
    }


    if (fd < 0)
        retstat = cloudfs_error("cloudfs_create!\n");
    ffi->fh = fd;
    return retstat;
}

/*
 * Functions supported by cloudfs 
 */
static struct fuse_operations cloudfs_operations = {
    .init = cloudfs_init,
    //
    // TODO
    //
    // This is where you add the VFS functions that your implementation of
    // MelangsFS will support, i.e. replace 'NULL' with 'melange_operation'
    // --- melange_getattr() and melange_init() show you what to do ...
    //
    // Different operations take different types of parameters. This list can
    // be found at the following URL:
    // --- http://fuse.sourceforge.net/doxygen/structfuse__operations.html
    //
    //
    .getattr = cloudfs_getattr,
    .getxattr = cloudfs_getxattr,
    .setxattr = cloudfs_setxattr,
    .mkdir = cloudfs_mkdir,
    .mknod = cloudfs_mknod,
    .open = cloudfs_open,
    .read = cloudfs_read,
    .write = cloudfs_write,
    .release = cloudfs_release,
    .opendir = cloudfs_opendir,
    .readdir = cloudfs_readdir,
    .access = cloudfs_access,
    .utime = cloudfs_utime,
    .chmod = cloudfs_chmod,
    .unlink = cloudfs_unlink,
    .rmdir = cloudfs_rmdir,
    .destroy = cloudfs_destroy,
    .readlink = cloudfs_readlink,
    .truncate = cloudfs_truncate,
    .chown = cloudfs_chown,
    .create = cloudfs_create
};

void cleaner(){
    if(!head_node){
        return;
    }
    struct file_info *cleaner_node = head_node;
    struct file_info *cleaner_prev = head_node;
    struct chunk *chunk_cleaner = NULL;
    struct chunk *chunk_cleaner_prev = NULL;
    while(cleaner_node){
        fprintf(logfile, "freeing: %s\n", cleaner_node->path);
        chunk_cleaner = cleaner_node->chunk_list;
        while(chunk_cleaner){
            fprintf(logfile, "freeing :%s\n", chunk_cleaner->md5);
            chunk_cleaner_prev = chunk_cleaner;
            chunk_cleaner = chunk_cleaner->next;
            free(chunk_cleaner_prev);
        }
        cleaner_prev = cleaner_node;
        cleaner_node = cleaner_node->next;
        free(cleaner_prev);
    }
}

int cloudfs_start(struct cloudfs_state *state,
                  const char *fuse_runtime_name)
{

    int argc = 0;
    char *argv[10];
    argv[argc] = (char *) malloc(128 * sizeof(char));
    strcpy(argv[argc++], fuse_runtime_name);
    argv[argc] = (char *) malloc(1024 * sizeof(char));
    strcpy(argv[argc++], state->fuse_path);
    argv[argc++] = "-s";        // set the fuse mode to single thread
    //argv[argc++] = "-f";        // run fuse in foreground 

    state_ = *state;
    logfile = fopen("./cloudfs.log", "w"); 
    // freopen("./cloudfs.log","w",stdout);
    // freopen("./stderr.log","w+",stderr);
    setvbuf(logfile, NULL, _IOLBF, 0); /* To ensure that a log of content is not buffered */ 
    fprintf(logfile, "%d\n", state_.threshold);
    fprintf(logfile, "cache size: %d\n", state_.cache_size);
    fprintf(logfile, "mkdir .cache: \n");
    cloudfs_mkdir("/.cache", 0755);
    
    int fuse_stat = fuse_main(argc, argv, &cloudfs_operations, NULL);

    fclose(logfile);
    cloud_destroy();
    cleaner();
    return fuse_stat;
}

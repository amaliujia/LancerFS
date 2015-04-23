#ifndef DUPLICATION_HPP
#define DUPLICATION_HPP

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
#include <openssl/md5.h>

//c++
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <string>

#include "Fuse.h"

#ifdef __cplusplus
extern "C"
{
#endif
    
#include "dedup.h"
#include "cloudapi.h"
    
#ifdef __cplusplus
}
#endif

#define PATH_LEN 4096
#define MD5_LEN ((MD5_DIGEST_LENGTH * 2) + 1)

using namespace std;

class duplication{
    
/*
This is a private class to wrap info of chunk, including md5 code and
segment length
*/
private:
    class MD5_code{
    public:
        char md5[MD5_LEN];
        int segment_len;
        
        /*
            set md5 code and len into class. This function can be used either
            initialize function or update function
         */
        void set_unsigned_code(unsigned char *code, int len){
            memset(md5, 0, MD5_LEN);
            for(int i = 0; i < MD5_DIGEST_LENGTH; i++){
                sprintf(&md5[i * 2], "%02x", code[i]);
            }
            segment_len = len;
        }
        
        void set_code(char *code){
            memcpy(md5, code, MD5_LEN);
        }
        
        void set_code(char *code, int len){
            memcpy(md5, code, MD5_LEN);
            segment_len = len;
        }
        
        /*
            override operater < and ==. in order to use stl map.
         */
        bool operator<(const MD5_code& other) const{
            for(int i = 0; i < MD5_LEN; i++){
                if(md5[i] == other.md5[i]){
                    continue;
                }else if(md5[i] < other.md5[i]){
                    return true;
                }else{
                    return false;
                }
            }
            return false;
        }
        
        bool operator==(const MD5_code& other) const{
            for(int i = 0; i < MD5_LEN; i++){
                if(md5[i] == other.md5[i]){
                    continue;
                }else{
                    return false;
                }
            }
            return true;
        }
    };
    
private:
    int window_size;
    int avg_seg_size;
    int min_seg_size;
    int max_seg_size;
    fuse_struct state_;
    
    rabinpoly_t *rp;
    FILE *logfd;
    
    // mapping from filename to chunks list
    map<string, vector<MD5_code> > file_map;
    
    // mapping from MD5 code to reference count
    map<MD5_code, int> chunk_set;
    
    set<string> cache_chunk;
    
public:
    duplication(FILE *fd, char *ssd_path);
    duplication(FILE *fd, fuse_struct *state);
    ~duplication();
    
    
    void deduplicate(const char *path);
    void retrieve(const char *fpath);
    void remove(const char *fpath);
    void clean(const char *fpath);
    
    int get_file_size(const char *fpath);
    int offset_read(const char *fpath, char *buf, size_t size, off_t offset);
    
private:
    void cloud_push_file(const char *fpath, struct stat *stat_buf);
    void cloud_get_shadow(const char *fullpath, const char *cloudpath);
    void cloud_push_shadow(const char *fpath);
    void init_rabin_structrue();
    void fingerprint(const char *path, vector<MD5_code> &code_list);
    void update_chunk(const char *fpath, vector<MD5_code> &code_list);
    void delete_chunk(const char *fpath, vector<MD5_code> &code_list);
    void log_msg(const char *format, ...);
    bool lookup();
    void serialization();
    void recovery();
    void put(const char *fpath, MD5_code &code, long offset);
    void get(const char *fpath, MD5_code &code, long offset);
    void get_in_buffer(MD5_code &code, char *fpath);
    void del(MD5_code &code);
    void ssd_fullpath(const char *path, char *fpath);
    void hidden_chunk_fullpath(const char *path, char *fpath);
    void cloud_filename(char *path);
    void get_local(const char *fpath, MD5_code &code, long offset);
};

#endif //DUPLICATION_HPP

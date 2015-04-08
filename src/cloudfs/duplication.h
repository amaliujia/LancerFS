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

using namespace std;

class duplication{
private:
  class MD5_code{
      public:
        unsigned char md5[MD5_DIGEST_LENGTH];
        int segment_len;
				
        void set_code(unsigned char *code, int len){
          memset(md5, 0, MD5_DIGEST_LENGTH);
          memcpy(md5, code, MD5_DIGEST_LENGTH);
          segment_len = len;
        }

        bool operator<(const MD5_code& other) const{
          for(int i = 0; i < MD5_DIGEST_LENGTH; i++){
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
           for(int i = 0; i < MD5_DIGEST_LENGTH; i++){
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
	//TODO: what data structure should I have 	
  int window_size;
  int avg_seg_size;
  int min_seg_size;
  int max_seg_size;
  char fname[PATH_MAX];
	
	rabinpoly_t *rp;
	FILE *logfd;
	
	map<string, vector<MD5_code> > file_map; 
	map<MD5_code, int> chunk_set;

public:
	duplication(FILE *fd);
	duplication(FILE *fd, int ws, int ass, int mss, int mxx);
	~duplication();
	
	void deduplicate(const char *path);
	void retrieve(const char *fpath);
	void remove(const char *fpath);
private:
	void init_rabin_structrue();
	void fingerprint(const char *path, vector<MD5_code> &code_list);
	void update_chunk(const char *fpath, vector<MD5_code> &code_list);
	void delete_chunk(const char *fpath, vector<MD5_code> &code_list);
	void log_msg(const char *format, ...);
	bool lookup();
	void serialization();
	void put(const char *fpath, MD5_code &code, long offset);
	void get(const char *fpath, MD5_code &code, long offset);	
	void del(MD5_code &code);
};

#endif //DUPLICATION_HPP

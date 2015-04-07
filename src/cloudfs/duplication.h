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
#include <unordered_map>
#include <unordered_set>

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
	//TODO: what data structure should I have 	
  int window_size;
  int avg_seg_size;
  int min_seg_size;
  int max_seg_size;
  char fname[PATH_MAX];
	
	rabinpoly_t *rp;
	FILE *logfd;
	
	std::unordered_map<string, std::vector<MD5_code> chunk_list> file_map; 
	std::map<MD5_code, int> chunk_set;

private:
	class file_list{
			public:
					std::vector<MD5_code>	chunk_list;	
	};	

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
public:
	duplication(FILE *fd);
	duplication(FILE *fd, int ws, int ass, int mss, int mxx);
	~duplication();
	
	void deduplicate(const char *path);

private:
	void init_rabin_structrue();
	void fingerprint(const char *path, vector<MD5_code> &code_list);
	void update_chunk(vector<MD5_code> &code_list);
	void log_msg(const char *format, ...);
	bool lookup();
	void serialization();
	void put(MD5_code &code);
	void get();	
};

#endif//DUPLICATION_HPP

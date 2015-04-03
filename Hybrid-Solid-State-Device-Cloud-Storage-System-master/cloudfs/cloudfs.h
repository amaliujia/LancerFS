#ifndef __CLOUDFS_H_
#define __CLOUDFS_H_

#define CACHE_RECORDS "/.record_cacheFiles"
#define HIDDEN_CACHE "/.cache" 
#define HASH_MAP_LOCATION "/.hashTable"
#define MAX_PATH_LEN 4096
#define MAX_HOSTNAME_LEN 1024

struct cloudfs_state {
  char ssd_path[MAX_PATH_LEN];
  char fuse_path[MAX_PATH_LEN];
  char hostname[MAX_HOSTNAME_LEN];
  int ssd_size;
  int threshold;
  int avg_seg_size;
  int rabin_window_size;
  int cache_size;
  char no_dedup;
  char no_cache;
};

void cloudBucketKey(const char* filepath, char* keyPath);
void retrieve_cachePath(char* cachePath, char* file);
int presentInCloud(const char* fullPath);

int cloudfs_start(struct cloudfs_state* state,
                  const char* fuse_runtime_name);
				  
void ssd_fullpath(char filepath[MAX_PATH_LEN], const char* path);
void proxyPath(char tempFile[MAX_PATH_LEN], const char* path);
void debug_msg(const char *format, ...);
int src_to_dest(char* src, char*dest);


#endif

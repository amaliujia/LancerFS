 #ifndef HEADER_FILE
 #define HEADER_FILE

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include <string.h>


void insert(char *key, int value);
void cache_map_insert(char * key, int lru, int size);

//if exists, return 1;
//else, return 0;
int exist(char *key);
int cache_map_exist(char * key);

void erase(char *key);
void cache_map_erase(char * key);

int get(char *key);
int cache_map_get_lru(char *key);
int cache_map_get_size(char *key);

void md5_map_it(char *path);
int cache_map_it(char *path);

void update_LRU(char *key);
char * eviction();

#ifdef __cplusplus
}
#endif

#endif
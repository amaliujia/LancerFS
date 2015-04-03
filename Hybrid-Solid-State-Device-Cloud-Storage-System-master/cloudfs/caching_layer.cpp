#include <stdio.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <cstring>
#include <list>
#include <vector>
#include "string.h"
#include <string>

/* Defined headers */
#include "cache_cloud_controller.h"
#include "cache_object_item.h"
#include "cloudfs.h"

/**
 * caching_layer: Constructor to initialize the cache
 */
caching_layer::caching_layer(deduplication_layer* storage, int capacity) {
  char filepath[MAX_PATH_LEN];
  ssd_fullpath(filepath, HIDDEN_CACHE);
  /* A hidden .cache folder is created to be used as a cache */
  mkdir(filepath, S_IRWXU);

  cache_size = capacity;
  curr_capacity = 0;
  num = 0;
  putIn_cloud = storage;
}

/**
 * trigger_cache: Function to start the cache
 */
void caching_layer::trigger_cache(){
  /*Code to construct the cache table*/

  int current_capacity = 0,dirtyBit;
  char record[MAX_PATH_LEN], filepath[MAX_PATH_LEN];
  off_t size;
  time_t access;

  ssd_fullpath(record, CACHE_RECORDS);
  FILE* fp = fopen(record, "r");

  if( fp != NULL ) {
    /* Obtaining the cache object details from the cache records */
    while(fscanf(fp, "%s %jd %lu %d", filepath, &size, &access, &dirtyBit) == 4) {
      string input(filepath);
      cache_object_item cacheObj(access, size, dirtyBit==1?true:false );
      putAtEnd(input, cacheObj);
      current_capacity += size;
    }
    fclose(fp); 
  }
  curr_capacity = current_capacity;
}


/** @brief retrieve- Retieve file from cache if present, 
 *                   else download from the cloud
 *
 *  @param input- Key to check if the file is in cache or not
 *  @param tempFile- Temporary buffer to read the file
 *
 */
void caching_layer::retrieve(string& input, char* tempFile){

  char pathToCache[MAX_PATH_LEN] = {0};
  char* filepath = (char* )input.c_str();

  retrieve_cachePath(pathToCache, tempFile);

  if( map_scan.find(input) == map_scan.end() )  {
    struct stat st;
    lgetxattr(filepath, "user.st_size", &st.st_size, sizeof(off_t));
    /* If the request file is too big to fit in the cache, directly download to ssd */
    if( st.st_size >= cache_size ) {
      putIn_cloud->pull_from_cloud(filepath, tempFile);
      return;
    } 
    /* Cache miss: Retrieve the file from cloud */
    putIn_cloud->pull_from_cloud(filepath, pathToCache);
    stat(pathToCache, &st);
    eviction(st.st_size);
    cache_object_item newCacheItem(st.st_atime, st.st_size, false);
    /* Latest cache object is always in the front */
    addToFront(input, newCacheItem);
    curr_capacity += newCacheItem.get_size();
    src_to_dest(pathToCache, tempFile);
  } else {
    /* Cached file position needs to be updated 
     * if the file is in cache 
     */
    retrieve(input);
    src_to_dest(pathToCache, tempFile);
  }

}


/** @brief dump_cache- Metadata of the cache is written into the record
 *                     for cache persistency.
 */
void caching_layer::dump_cache(){

  char record[MAX_PATH_LEN];
  ssd_fullpath(record, CACHE_RECORDS);

  FILE* fp = fopen(record, "w");
  list< pair<string, cache_object_item> >::iterator iter;

  for(iter = cache_LinkedList.begin(); iter!= cache_LinkedList.end(); iter++) {
    int temp;
    if((iter->second).dirtyBit)
      temp=1;
    else
      temp=0;
    fprintf(fp, "%s %jd %lu %d\n", (iter->first).c_str(), 
        (iter->second).get_size(), (iter->second).get_access(),temp); 
  }
  fclose(fp); 
}


/** @brief put- Store the file in cache
*/

void caching_layer::put(string& input, char* tempFile) {

  char* filepath =(char*)input.c_str();
  struct stat st;
  stat(tempFile, &st);

  /* If file size is greater than cache size, put in cloud  */
  if(st.st_size >= cache_size) {
    putIn_cloud->putSegmentsInCloud(filepath, tempFile);
    rename(tempFile, filepath);
    return;    
  }
  cache_object_item value(st.st_atime, st.st_size, true);
  if( map_scan.find(input) == map_scan.end())
    curr_capacity += value.get_size();

  eviction(value.get_size());
  addToFront(input, value);
  putIn_cloud->upload_to_cache(filepath,tempFile);

}


/** @brief contains- Present in the cache or not
*/
bool caching_layer::contains(string& input) {
  if(map_scan.find(input) != map_scan.end())
    return true;

  return false;
}


/** @brief erase- Delete a cache object
*/

void caching_layer::erase(string& input) {

  map< string,  list< pair<string, cache_object_item> >::iterator >:: iterator iter;
  iter = map_scan.find(input);
  /* If the cache object to be deleted is found, remove it from the List DS */
  if(iter != map_scan.end()) {
    num--;
    cache_LinkedList.erase(iter->second);
    map_scan.erase(iter);
  }
}


/* Least recenty used policy */
void caching_layer::addToFront(string& input, cache_object_item& value) {
  map< string,  list< pair<string, cache_object_item> >::iterator >:: iterator iter;
  iter = map_scan.find(input);
  if( iter != map_scan.end()) {
    cache_LinkedList.erase(iter->second);
    map_scan.erase(iter);
  } else
    num ++;

  cache_LinkedList.push_front(make_pair(input, value));
  map_scan.insert(make_pair(input, cache_LinkedList.begin())); 
}

void caching_layer::retrieve(string& input) {
  map< string,  list< pair<string, cache_object_item> >::iterator >:: iterator iter;
  iter = map_scan.find(input);
  cache_LinkedList.splice(cache_LinkedList.begin(), cache_LinkedList, iter->second);
}


void caching_layer::putAtEnd(string& input, cache_object_item& value) {
  map< string,  list< pair<string, cache_object_item> >::iterator >:: iterator iter;
  iter = map_scan.find(input);
  if( iter != map_scan.end()) {
    cache_LinkedList.erase(iter->second);
    map_scan.erase(iter);
  } else
    num ++;

  cache_LinkedList.push_back(make_pair(input, value));
  map_scan.insert(make_pair(input, --cache_LinkedList.end())); 
}


/** @brief eviction: Create space by evicting old cache entries
*/
void caching_layer::eviction(off_t size){
  /* Array of filepath inputs */
  vector<string> inputs;
  list< pair<string, cache_object_item> >::reverse_iterator iter;

  for(iter = cache_LinkedList.rbegin(); curr_capacity + size >(unsigned int)cache_size && iter != cache_LinkedList.rend(); iter++) {
    char tempFile[MAX_PATH_LEN] = {0};
    char pathToCache[MAX_PATH_LEN] = {0};

    string key(iter->first);
    inputs.push_back(key);
    cache_object_item cacheObj = iter->second;  
    curr_capacity -= cacheObj.get_size();
    char* filepath =(char*)key.c_str();

    proxyPath(tempFile, filepath);
    retrieve_cachePath(pathToCache, tempFile);

    /* Delete from cache folder when not modified(dirty) */  
    if(!cacheObj.dirtyBit)
      unlink(pathToCache);
    else {   
      struct stat st1,st;
      stat(pathToCache, &st);
      stat(filepath, &st1);
      putIn_cloud->build_pathTohashMap(filepath);
      putIn_cloud->putSegmentsInCloud(filepath, pathToCache);
      rename(pathToCache, filepath);

      int presentCloud = 1;
      lsetxattr(filepath, "user.presentCloud", &presentCloud, sizeof(int), 0);
      lsetxattr(filepath, "user.st_size", &st.st_size, sizeof(off_t), 0);
      lsetxattr(filepath, "user.st_atime", &st1.st_atime, sizeof(time_t), 0);
      lsetxattr(filepath, "user.st_mtime", &st1.st_mtime, sizeof(time_t), 0);
      lsetxattr(filepath, "user.st_ctime", &st1.st_ctime, sizeof(time_t), 0);
      lsetxattr(filepath, "user.st_blocks", &st.st_blksize, sizeof(blkcnt_t), 0);

    } 
  }
  map< string,  list< pair<string, cache_object_item> >::iterator >:: iterator iter2;
  for( int i = 0; i < inputs.size(); i++) {
    iter2 = map_scan.find(inputs[i]);
    cache_LinkedList.erase(iter2->second);
    map_scan.erase(iter2);
  }
  num -= inputs.size();
}


/*
 * cache deconstructor
 */
caching_layer::~caching_layer() {
  dump_cache();
  map_scan.clear();
  cache_LinkedList.clear();
}

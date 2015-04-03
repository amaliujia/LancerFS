#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <iostream>

/* Defined headers*/
#include "cloudfs.h"
#include "cache_cloud_controller.h"

/**
 * cache_cloud_controller: Constructor to initialize the cache
 */
cache_cloud_controller::cache_cloud_controller(struct cloudfs_state* state_) {
  fs_state = state_;
  putIn_cloud = new deduplication_layer(state_);
  if(!fs_state->no_cache) {
    cacheAccess = new caching_layer(putIn_cloud, state_->cache_size);
    cacheAccess->trigger_cache();
  }
}


/**
 *	retrieve: Retrieve the file from either cloud or cache
 *	          based on no_cache option
 */
void cache_cloud_controller::retrieve(char* filepath, char* tempFile) {
  if(fs_state->no_cache)
    putIn_cloud->pull_from_cloud(filepath, tempFile);
  else {
    string input(filepath);
    cacheAccess->retrieve(input, tempFile);
  }
}

/**
 *	remove_from_cloud: Function to delete the file from cloud
 */
void cache_cloud_controller::remove_from_cloud(char* filepath) {
  if(!fs_state->no_cache) {
    char tempFile[MAX_PATH_LEN] = {0};
    char location_cache[MAX_PATH_LEN] = {0};
    string input(filepath);

    struct stat st;
    stat(filepath, &st);
    proxyPath(tempFile, filepath);

    retrieve_cachePath(location_cache, tempFile);
    if ( cacheAccess->contains(input) ) {
      /* Remove from cache if present */
      cacheAccess->erase(input);
      unlink(location_cache);
    } 
    if(st.st_size != 0)
      putIn_cloud->remove_from_cloud(filepath);
  } else
    putIn_cloud->remove_from_cloud(filepath);
}

/**
 * update - Check whether cloud has to be accessed or the cache
 * @param  filepath - Full file path is used the key
 * @param  tempFile - Temporary file used as buffer
 */
  void cache_cloud_controller::update(char* filepath, char* tempFile) {
    if(fs_state->no_cache)
      putIn_cloud->putSegmentsInCloud(filepath, tempFile);
    else {
      string input(filepath);
      cacheAccess->put(input, tempFile);
    }  
  }


/**
 * cache_cloud_controller: Destructor
 */
cache_cloud_controller::~cache_cloud_controller() {
  delete putIn_cloud;
  if(cacheAccess != NULL)
    delete cacheAccess;
}

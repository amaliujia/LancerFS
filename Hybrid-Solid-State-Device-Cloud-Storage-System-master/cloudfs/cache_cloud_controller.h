#ifndef __CLOUDFS_BACKEND_H
#define __CLOUDFS_BACKEND_H

#include <map>
#include <list>
#include <string>

/* Defined headers*/
#include "cloudapi.h"
#include "cloudfs.h"
#include "dedup.h"
#include "cache_object_item.h"

using namespace std;


class deduplication_layer {
  private:
    static struct cloudfs_state* fs_state;
    /* Datastructures: Hash to reference count */
    map<string, int> hashTableMap;
    map<string, map<string, int> > pathTohashMap;

    int min_seg_size, max_seg_size;
    static int infile;
    static int bytes_remaining;
    static int segmentBegin;
    static int outfile;
    static char* readSegment;
    static int get_buffer(const char *buffer, int bufferLength);
    static int put_buffer(char *buffer, int bufferLength);

  public:
    deduplication_layer(struct cloudfs_state* state_);
    ~deduplication_layer();
    void remove_from_cloud(char* filepath);
    void check_hashtable_entry(char *hashValue, int segment_len, int *retstat, char *filepath);
    void hashTable_to_SSD();
    void build_hashTable();
    void putSegmentsInCloud(char* filepath, char* tempFile);
    void upload_to_cache(char* filepath, char* tempFile);
    void pull_from_cloud(char* filepath, char* tempFile);
    void build_pathTohashMap(char* filepath);

};

class caching_layer {
  private:
    map<string, list< pair<string, cache_object_item> >::iterator> map_scan;
    int cache_size,curr_capacity, num;
    list< pair<string, cache_object_item> > cache_LinkedList;
    deduplication_layer* putIn_cloud;
    void putAtEnd(string& key, cache_object_item& value);
    void addToFront(string& key, cache_object_item& value);
    void retrieve(string& key);

  public:
    caching_layer(deduplication_layer* putIn_cloud, int capacity);
    ~caching_layer();
    void dump_cache();
    void trigger_cache();
    void put(string& key, char* tempFile);
    void eviction(off_t size);
    bool contains(string& key);
    void erase(string& key);
    void retrieve(string& key, char* tempFile);
};

class cache_cloud_controller {
  private:
    struct cloudfs_state* fs_state;
    deduplication_layer* putIn_cloud;
    caching_layer* cacheAccess;

  public:
    cache_cloud_controller(struct cloudfs_state* state_);
    ~cache_cloud_controller();
    void remove_from_cloud(char* filepath);
    void update(char* filepath, char* tempFile);
    void retrieve(char* filepath, char* tempFile);

};

#endif

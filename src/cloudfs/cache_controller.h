//
//  cache_controller.h
//  


#ifndef CACHE_CONTROLLER_HPP
#define CACHE_CONTROLLER_HPP

#include "Fuse.h"
#include "lock.h"
#include "log.h"

using namespace std;

class cache_controller {
private:
    set<string> chunk_cache;
    map<string, int> chunk_read_cache;
    map<string, vector<string> > chunk_write_cache;

    fsLock _mutex;
    fsLock _mutex_read;
    fsLock _mutex_write;

public:
    cache_controller();

    ~cache_controller();

    int lookup(const char *chunk_name);

    void cache_read(const char *chunk_name);

    void cache_write(const char *filename, const char *chunk_name);

    void release_read(const char *chunk_name);

    void release_write(const char *filename, const char *chunk_name);

    void garbage_collect();

    int size();

    int size_read_cache();

    int size_write_cache();
};


#endif /* CACHE_CONTROLLER_HPP */

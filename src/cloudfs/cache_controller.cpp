//
//  cache_controller.cpp
//  


#include "cache_controller.h"

cache_controller::cache_controller() {

}

cache_controller::~cache_controller() {
    garbage_collect();
}

int cache_controller::size() {
    return chunk_cache.size();
}

int cache_controller::size_read_cache() {
    return chunk_read_cache.size();
}

int cache_controller::lookup(const char *chunk_name) {
    set<string>::iterator iter;
    string s(chunk_name);
    _mutex.get();
    iter = chunk_cache.find(s);
    int result = (iter != chunk_cache.end() ? 1 : 0);
    _mutex.release();
    return result;
}

void cache_controller::cache_read(const char *chunk_name) {
    map<string, int>::iterator iter_read;
    string s(chunk_name);
    _mutex_read.get();
    if ((iter_read = chunk_read_cache.find(s)) != chunk_read_cache.end()) {
        int ref = iter_read->second;
        chunk_read_cache[s] = ref + 1;
    } else {
        chunk_read_cache.insert(pair<string, int>(s, 1));
    }
    _mutex_read.release();
}

void cache_controller::release_read(const char *chunk_name) {
    map<string, int>::iterator iter_read;
    string s(chunk_name);
    _mutex_read.get();
    if ((iter_read = chunk_read_cache.find(s)) != chunk_read_cache.end()) {
        int ref = iter_read->second;
        if (ref > 1)
            chunk_read_cache[s] = ref - 1;
        else
            chunk_read_cache.erase(iter_read);
    } else {
        log_msg("LancerFS error: release a cached chunk %s, which \
							doesn't exist\n", chunk_name);
    }
    _mutex_read.release();
}

void cache_controller::garbage_collect() {
    map<string, int>::iterator iter_read;

    _mutex_read.get();

    //critical secton
    for (iter_read = chunk_read_cache.begin();
         iter_read != chunk_read_cache.end();
         iter_read++) {
        if (iter_read->second == 0) {
            chunk_read_cache.erase(iter_read);
        }
    }
    _mutex_read.release();
}

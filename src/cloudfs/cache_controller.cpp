//
//  cache_controller.cpp
//  


#include "cache_controller.h"

cache_controller::cache_controller(){
    
}

cache_controller::~cache_controller(){
    
}

int cache_controller::lookup(const char *chunk_name){
    set<string>::iterator iter;
    string s(chunk_name);
    iter = chunk_cache.find(s);
    return iter != chunk_cache.end() ? 1 : 0;
}

void cache_controller::cache_read(const char *chunk_name){
    map<string, int>::iterator iter_read;
    string s(chunk_name);
    if((iter_read = chunk_read_cache.find(s)) != chunk_read_cache.end()){
        int ref = iter_read->second;
        chunk_read_cache[s] = ref + 1;
    }else{
        chunk_read_cache.insert(pair<string, int>(s, 1));
    }
}

void 

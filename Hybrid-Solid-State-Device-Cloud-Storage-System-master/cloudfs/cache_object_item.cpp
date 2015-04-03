#include <string>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <iostream>

#include "cache_object_item.h"


cache_object_item::cache_object_item(time_t accessTime, off_t cacheSize, bool set) {

  this->size = cacheSize; // Size of the cache
  this->access = accessTime; // Time at which the cache is accessed
  this->dirtyBit = set; // Check if the cache object is modified or not
}

cache_object_item::cache_object_item(){}

/*	Setting the acces time */
void cache_object_item::set_access(time_t accessTime) {
  access = accessTime;
}

/*	Getting the access time */
time_t cache_object_item::get_access() {
  return access;
}

/* Getting the file size */
off_t cache_object_item::get_size() {
  return size;
}

/* Comparing the cache objects based in access time and size */
bool cache_object_item::operator > (cache_object_item obj) {

  if( size > obj.get_size())
    return false;
  else if( difftime(access, obj.get_access() ) > 3 )
    return true;
  else 
    return true;

  return false;
}

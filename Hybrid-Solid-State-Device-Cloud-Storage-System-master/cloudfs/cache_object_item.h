#ifndef __CACHE_ELEMENT_H
#define __CACHE_ELEMENT_H

#include <sys/stat.h>
#include <time.h>

class cache_object_item {

  public:
    off_t size;
    time_t access;
    bool dirtyBit;

    /* Functions */
    cache_object_item();
    cache_object_item(time_t access, off_t size, bool set);
    time_t get_access();

    void set_dirty(bool b);
    off_t get_size();

    void set_access(time_t access);
    bool operator > (cache_object_item);
};
#endif

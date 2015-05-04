//
//  lock.h
//
//

#ifndef LOCK_HPP
#define LOCK_HPP

#include "Fuse.h"

class fsLock{
private:
    pthread_mutex_t _lock;

public:
    fsLock();
    ~fsLock();
    
    void get();
    bool try_get();
    void release();
    
};

#endif /* LOCK_HPP */

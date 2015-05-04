//
//  lock.cpp
//  


#include "lock.h"

fsLock::fsLock(){
    pthread_mutex_init(&_lock, 0);
}

fsLock::~fsLock(){
    pthread_mutex_destroy(&_lock);
}

void fsLock::get(){
    pthread_mutex_lock(&_lock);
}

bool fsLock::try_get(){
    return pthread_mutex_trylock(&_lock);
}

void fsLock::release(){
    pthread_mutex_unlock(&_lock);
}

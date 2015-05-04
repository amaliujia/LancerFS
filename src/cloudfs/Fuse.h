#ifndef FUSE_HPP
#define FUSE_HPP

//c
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <stdarg.h>
#include <openssl/md5.h>

//c++
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <string>

#include "wrapper.h"

#define MAX_PATH_LEN 4096
#define MAX_HOSTNAME_LEN 1024

#define UNUSED __attribute__((unused))

typedef unsigned long TIMESTAMP;

/*
 This class is wrapper of fuse_struct, used into object-oriented programming.
 */
class fuse_struct{
public:
    char ssd_path[MAX_PATH_LEN];
    char fuse_path[MAX_PATH_LEN];
    char hostname[MAX_HOSTNAME_LEN];
    int ssd_size;
    int threshold;
    int avg_seg_size;
    int rabin_window_size;
    char no_dedup;
    
public:
    void init(struct cloudfs_state *state){
        strcpy(ssd_path, state->ssd_path);
        strcpy(fuse_path, state->fuse_path);
        strcpy(hostname, state->hostname);
        ssd_size = state->ssd_size;
        threshold = state->threshold;
        avg_seg_size = state->avg_seg_size;
        rabin_window_size = state->rabin_window_size;
        no_dedup = state->no_dedup;
    }
    
    void copy(fuse_struct *state){
        strcpy(ssd_path, state->ssd_path);
        strcpy(fuse_path, state->fuse_path);
        strcpy(hostname, state->hostname);
        ssd_size = state->ssd_size;
        threshold = state->threshold;
        avg_seg_size = state->avg_seg_size;
        rabin_window_size = state->rabin_window_size;
        no_dedup = state->no_dedup;
    }
};

#endif

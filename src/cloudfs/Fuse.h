#ifndef FUSE_HPP
#define FUSE_HPP

#include "wrapper.h"

#define MAX_PATH_LEN 4096
#define MAX_HOSTNAME_LEN 1024

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

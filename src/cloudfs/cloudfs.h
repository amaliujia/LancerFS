#ifndef __CLOUDFS_H_
#define __CLOUDFS_H_


#include "wrapper.h"

int cloudfs_start(struct cloudfs_state *state, const char *fuse_runtime_name);

void cloudfs_get_fullpath(const char *path, char *fullpath);

#endif

#ifndef TRANSMISSION_HPP__
#define TRANSMISSION_HPP__

//c
#include <ctype.h>
#include <dirent.h>
#include <fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include "cloudapi.h"

#ifdef __cplusplus
}
#endif


void push_to_cloud(const char *cloudpath, const char *filename);
void get_from_cloud(const char *bucket,
                    const char *cloudpath, const char *filename);
void delete_object(const char *bucket, const char *cloudpath); 

#endif

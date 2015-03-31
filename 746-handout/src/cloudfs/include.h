#ifndef INCLUDE_H__
#define INCLUDE_H__

#define MAX_PATH_LEN 4096
#define MAX_HOSTNAME_LEN 1024
#define MAX_MSG_LEN 128

//c
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

static FILE *logfd = NULL;

#endif

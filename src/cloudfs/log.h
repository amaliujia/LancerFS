#ifndef LOG_H
#define LOG_H

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

void log_init(const char *logpath);

void log_msg(const char *format, ...);

void log_destroy();


#endif // LOG_HPP

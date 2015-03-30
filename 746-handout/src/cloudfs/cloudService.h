#ifndef CLOUD_SERVICE_H__
#define CLOUD_SERVICE_H__

#include "include.h"
#include "cloudapi.h"

static FILE *outfile;
static FILE *infile;

void cloud_filename(char *path);
void cloud_slave_filename(char *path);
int list_bucket(const char *key, time_t modified_time, uint64_t size);
int list_service(const char *bucketName);
int get_buffer(const char *buffer, int bufferLength);
int put_buffer(char *buffer, int bufferLength);


void cloud_push_file(const char *path, struct stat *stat_buf);
void cloud_push_shadow(const char *fullpath, const char *shadowpath, struct stat *stat_buf); 
void cloud_get_shadow(const char *fullpath, const char *cloudpath, int *fd); 

#endif

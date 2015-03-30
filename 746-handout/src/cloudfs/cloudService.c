#include "cloudService.h"

void cloud_filename(char *path){
  //TODO: bug may exist
  while(path != '\0'){
      if(*path == '\\'){
          *path = '+';
      }
      path++;
  }
}

void cloud_slave_filename(char *path){
	sprintf(path, "%s%s", path, ".sl");	
}

int list_bucket(const char *key, time_t modified_time, uint64_t size) {
  fprintf(logfd, "list bucket: %s %lu %llu\n", key, modified_time, size);
  return 0;
}

int list_service(const char *bucketName) {
  fprintf(logfd, "bucketname: %s\n", bucketName);
  return 0;
}

int get_buffer(const char *buffer, int bufferLength) {
  return fwrite(buffer, 1, bufferLength, outfile);
}

int put_buffer(char *buffer, int bufferLength) {
  fprintf(logfd, "put_buffer %d \n", bufferLength);
  return fread(buffer, 1, bufferLength, infile);
}

//Confirm if influence open/close operation here 

void cloud_push_file(const char *path, struct stat *stat_buf){
	infile = fopen(path, "rb");
	if(infile == NULL){
			fprintf(logfd, "LancerFS error: cloud push %s failed\n", path);
			return;		
	}
	fprintf(logfd, "LancerFS log: cloud_push_file(path=%s)\n", path);
  lstat(path, stat_buf);
  cloud_put_object("bkt", path, stat_buf->st_size, put_buffer);
  fclose(infile);	
}

//TODO: confirm if api use correctly
void cloud_push_shadow(const char *fullpath, const char *shadowpath, struct stat *stat_buf){
	char cloudpath[MAX_PATH_LEN+3];
	memset(cloudpath, 0, MAX_PATH_LEN+3);
	strcpy(cloudpath, fullpath);
	infile = fopen(shadowpath, "rb");
  if(infile == NULL){
      fprintf(logfd, "LancerFS error: cloud push %s failed, cloudpath %s, \ 
				shadowpath %s\n", fullpath, cloudpath, shadowpath);
      return;
  }	

	cloud_filename(cloudpath);
  fprintf(logfd, "LancerFS log: cloud_push_file(path=%s)\n", cloudpath);
  lstat(cloudpath, stat_buf);
  cloud_put_object("bkt", cloudpath, stat_buf->st_size, put_buffer);
  fclose(infile);
}

void cloud_get_shadow(const char *fullpath, const char *cloudpath, int *fd){
  outfile = fopen(fullpath, "wb");
  cloud_get_object("bkt", cloudpath, get_buffer);
	*fd = outfile;
} 
	 

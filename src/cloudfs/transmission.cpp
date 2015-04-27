#include "transmission.h"

static  FILE *outf;
static  FILE *inf;

int g_buffer(const char *buffer, int bufferLength) {
    return fwrite(buffer, 1, bufferLength, outf);
}

int p_buffer(char *buffer, int bufferLength) {
    return fread(buffer, 1, bufferLength, inf);
}

void push_to_cloud(const char *cloudpath, const char *filename){
		inf = fopen(filename, "rb");	  
		if(inf == NULL){
        //log_msg("LancerFS error: cloud push %s failed\n", fpath);
        return;
    }
		struct stat stat_buf;
		lstat(filename, &stat_buf);	
		cloud_put_object("snapshot", cloudpath, stat_buf.st_size, p_buffer); 
}

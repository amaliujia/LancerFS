#include <stdio.h>
#include <stdlib.h>

static char *logpath = "/home/student/LancerFS/746-handout/src/log/trace.log";
static FILE *logfd = NULL;

int main(){
		int ret = 0;
		ret = mkdir("ssdtmp", 0x777); 
		logfd = fopen(logpath, "w+");
		if(ret < 0){
			printf("cannot make directory %s\n", "ssdtmp");
		}else{
			printf("Successfully create!");
			fprintf(logfd, "Successfully create!");
		}
		return ret;
}

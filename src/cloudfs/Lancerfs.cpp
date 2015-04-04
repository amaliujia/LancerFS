#include "Lancerfs.h"

void LancerFS::cloudfs_get_fullpath(const char *path, char *fullpath){
	sprintf(fullpath, "%s", state_.ssd_path);
	path++; 
	sprintf(fullpath, "%s%s", fullpath, path);	
}

void LancerFS::log_msg(const char *format, ...){
    va_list ap;
    va_start(ap, format);
    vfprintf(logfd, format, ap);
    fflush(logfd);
}

int LancerFS::cloudfs_error(char *error_str)
{
    int retval = -errno;
    log_msg(error_str);
    return retval;
}

//LancerFS* LancerFS::_lancerFS = NULL;

LancerFS::LancerFS(){
	char *logpath = "/tmp/cloudfs.log";

	//init log
	logfd = fopen(logpath, "w");
	if(logfd == NULL){
		printf("LancerFS Error: connot find log file\n");
		exit(1);	
	}

	//init cloud	

}

LancerFS::~LancerFS(){
	fclose(logfd);
}

int LancerFS::lgetattr(const char *path, struct stat *statbuf){
	
}

int LancerFS::lreaddir(const char *path, void *buf, fuse_fill_dir_t filler, 
							off_t offset, struct fuse_file_info *fi)
{
	int ret = 0;
	DIR *dp;
	struct dirent *de;

  log_msg("\ncfs_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x,  \
					offset=%lld, fi=0x%08x)\n", path, buf, filler, offset, fi);	
	dp = (DIR *)(uintptr_t) fi->fh;
	
	de = readdir(dp);
	if(de == 0){
		//ret = cloudfs_error("readdir fail\n");
		return ret;	
	}

	do{
		if(strcmp(de->d_name, "lost+found") == 0){
			continue;
		}
		log_msg("calling filler with name %s\n", de->d_name);
		if(filler(buf, de->d_name, NULL, 0) != 0){
			return -ENOMEM;
		}
	}while((de = readdir(dp)) != NULL);
	
	return ret;
}

int LancerFS::lmkdir(const char *path, mode_t mode){
	int ret = 0;
  char fpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fpath);

  log_msg("\ncfd_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);
	
	ret = mkdir(fpath, mode);
	if(ret < 0){
			//ret = cloudfs_error("mkdir fail\n");
	}
	return ret;
}


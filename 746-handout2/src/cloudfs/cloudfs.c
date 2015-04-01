//#define _BSD_SOURCE 
#define _XOPEN_SOURCE 500
#define _ATFILE_SOURCE 

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
#include <stdarg.h>

#include "cloudapi.h"
#include "cloudfs.h"
#include "dedup.h"

#define UNUSED __attribute__((unused))

static char *logpath = "/home/student/LancerFS/746-handout/src/log/trace.log";
static FILE *logfd = NULL ;

static struct cloudfs_state state_;

void cloudfs_get_fullpath(const char *path, char *fullpath){
	sprintf(fullpath, "%s", state_.ssd_path);
	path++; 
	sprintf(fullpath, "%s%s", fullpath, path);	
}

void log_msg(const char *format, ...){
    va_list ap;
    va_start(ap, format);
    vfprintf(logfd, format, ap);
		fflush(logfd);
}

static int cloudfs_error(char *error_str)
{
    int retval = -errno;
   	log_msg(error_str); 
    //fprintf(stderr, "CloudFS Error: %s\n", error_str);

    return retval;
}

void cloudfs_log_close(){
	fclose(logfd);
}

void cloudfs_log_init(){
	logfd = fopen(logpath, "w");
	if(logfd == NULL){
		printf("LancerFS Error: connot find log file\n");
		exit(1);	
	}
}

/*
 * Initializes the FUSE file system (cloudfs) by checking if the mount points
 * are valid, and if all is well, it mounts the file system ready for usage.
 *
 */
void *cloudfs_init(struct fuse_conn_info *conn)
{
  cloud_init(state_.hostname);
  return NULL;
}

void cloudfs_destroy(void *data) {
  cloud_destroy();
}

int cloudfs_getattr(const char *path , struct stat *statbuf)
{
  int ret = 0;
	char fpath[MAX_PATH_LEN];
	cloudfs_get_fullpath(path, fpath);	

  log_msg("\ncfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
					path, statbuf);
	ret = lstat(fpath, statbuf);	
	if(ret != 0){
			ret = cloudfs_error("getattr lstat\n");
	}
  return ret;
}

int cloudfs_mkdir(const char *path, mode_t mode){
	int ret = 0;
  char fpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fpath);

    log_msg("\ncfd_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);
	ret = mkdir(fpath, mode);
	if(ret < 0){
			ret = cloudfs_error("mkdir fail\n");
	}
	return ret;
}

int cloudfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
	int ret = 0;
	DIR *dp;
	struct dirent *de;

  log_msg("\ncfs_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n", path, buf, filler, offset, fi);	
	dp = (DIR *)(uintptr_t) fi->fh;
	
	de = readdir(dp);
	if(de == 0){
		ret = cloudfs_error("readdir fail\n");
		return ret;	
	}

	do{
		log_msg("calling filler with name %s\n", de->d_name);
		if(filler(buf, de->d_name, NULL, 0) != 0){
			return -ENOMEM;
		}
	}while((de = readdir(dp)) != NULL);
		
	return ret;	
}

int cloudfs_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int ret = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("\ncfs_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = %d)\n",
	    path, name, value, size);
    cloudfs_get_fullpath(path, fpath);
    
    ret = lgetxattr(fpath, name, value, size);
    if (ret < 0){
				ret = cloudfs_error("cfs_getxattr lgetxattr");
		}else{
				log_msg("    value = \"%s\"\n", value);
   	} 
    return ret;
}

int cloudfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags){
	int ret = 0;
	char fpath[MAX_PATH_LEN];	
  log_msg("\ncfs_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n", path, name, value, size, flags);
  cloudfs_get_fullpath(path, fpath);
   
  ret = lsetxattr(fpath, name, value, size, flags);
  if (ret < 0)
			ret = cloudfs_error("cfs_setxattr lsetxattr");
   
  return ret;
}


int cloudfs_access(const char *path, int mask)
{
  int ret = 0;
  char fpath[MAX_PATH_LEN];
   
  log_msg("\ncfs_access(path=\"%s\", mask=0%o)\n", path, mask);
  cloudfs_get_fullpath(path, fpath);
    
  ret = access(fpath, mask);
    
  if(ret < 0)
		ret = cloudfs_error("cfs_access access");
   return ret;
}

int cloudfs_mknod(const char *path, mode_t mode, dev_t dev){
	int ret = 0;
	char fpath[MAX_PATH_LEN];

  log_msg("\nmknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
	path, mode, dev);
	cloudfs_get_fullpath(path, fpath);
	
  if(S_ISREG(mode)){
     ret = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
		if(ret < 0)
	    ret = cloudfs_error("mknod open\n");
    else{
      ret = close(ret);
	  	if(ret < 0)
		  	 ret = cloudfs_error("mknod close\n");
	   }
  }else if(S_ISFIFO(mode)){
	    ret = mkfifo(fpath, mode);
	    if(ret < 0)
				ret = cloudfs_error("mknod mkfifo\n");
	}else{
	    ret = mknod(fpath, mode, dev);
	    if(ret < 0)
				ret = cloudfs_error("mknod mknod\n");
	}
 
	 return ret;	
}

int cloudfs_open(const char *path, struct fuse_file_info *fi){
	int ret = 0;
	int fd;
	char fpath[MAX_PATH_LEN];
		
  log_msg("\ncfd_open(path\"%s\", fi=0x%08x)\n", path, fi);
  cloudfs_get_fullpath(path, fpath);

  fd = open(fpath, fi->flags);
  if (fd < 0)
		ret = cloudfs_error("open fail\n");
 
  fi->fh = fd;
  return ret;	
}

int cloudfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\ncfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);
    
    retstat = pread(fi->fh, buf, size, offset);
    if (retstat < 0)
				retstat = cloudfs_error("cfs_read read");
    
    return retstat;
}

int cloudfs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi){
	int ret = 0;
  log_msg("\ncfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);
	
  ret = pwrite(fi->fh, buf, size, offset);
  if(ret < 0)
		ret = cloudfs_error("pwrite fail\n");
    
   return ret;
}

int cloudfs_release(const char *path, struct fuse_file_info *fi){
	int ret = 0;
	
  log_msg("\ncfs_release(path=\"%s\", fi=0x%08x)\n", path, fi);
	
	ret = close(fi->fh);
	return ret;
}

int cloudfs_opendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp;
    int ret = 0;
    char fpath[PATH_MAX];
    
    log_msg("\ncfs_opendir(path=\"%s\", fi=0x%08x)\n", path, fi);
    cloudfs_get_fullpath(path, fpath);
    
    dp = opendir(fpath);
    if (dp == NULL)
			ret = cloudfs_error("opendir fail\n");
    
    fi->fh = (intptr_t) dp;
    return ret;
}

int cloudfs_utimens(const char *path, const struct timespec tv[2]){
  int ret = 0;
  char fullpath[MAX_PATH_LEN];
  cloudfs_get_fullpath(path, fullpath);

	log_msg("Lancer log: cfs_utimens(path=%s)\n", fullpath);
	ret = utimensat(0, fullpath, tv, 0);
	if(ret < 0){
		ret = cloudfs_error("utimes fail\n");
	}			
	return ret;	
}

int cloudfs_chmod(const char *path, mode_t mode)
{
  int ret = 0;
  char fpath[MAX_PATH_LEN];
    
  log_msg("\ncfs_chmod(fpath=\"%s\", mode=0%03o)\n", path, mode);
  cloudfs_get_fullpath(path, fpath); 
    
	ret = chmod(fpath, mode);
 	if(ret < 0)
		ret = cloudfs_error("chmod fail\n");
    
  return ret;
}

int cloudfs_unlink(const char *path)
{
    int retstat = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("cfs_unlink(path=\"%s\")\n",
	    path);
    cloudfs_get_fullpath(path, fpath);
    
    retstat = unlink(fpath);
    if (retstat < 0)
				retstat = cloudfs_error("unlink fail");
    
    return retstat;
}

int cloudfs_rmdir(const char *path){
    int retstat = 0;
    char fpath[MAX_PATH_LEN];
    
    log_msg("cfs_rmdir(path=\"%s\")\n", path);
    cloudfs_get_fullpath(path, fpath);
    
    retstat = rmdir(fpath);
    if (retstat < 0)
			retstat = cloudfs_error("cfs_rmdir rmdir");
    
    return retstat;
}
/*
 * Functions supported by cloudfs 
 */
static struct fuse_operations cloudfs_operations = {
    .init           = cloudfs_init,
    .getattr        = cloudfs_getattr,
    .mkdir          = cloudfs_mkdir,
    .readdir        = cloudfs_readdir,
    .destroy        = cloudfs_destroy,
		.getxattr				= cloudfs_getxattr,
		.setxattr				= cloudfs_setxattr,
		.access					= cloudfs_access,
		.mknod					=	cloudfs_mknod,
		.open						= cloudfs_open,
		.read						= cloudfs_read,
		.write					= cloudfs_write,
		.release				= cloudfs_release,
		.opendir				= cloudfs_opendir,
		.utimens				= cloudfs_utimens,
		.chmod					= cloudfs_chmod,
		.unlink					= cloudfs_unlink,
		.rmdir					= cloudfs_rmdir,

		.truncate				= truncate,
		.readlink				=	readlink,
		.symlink				=	symlink,
		.rename					=	rename,
		.link						= link,
		.chown					=	chown,
		.listxattr			=	listxattr,
		.removexattr		=	removexattr,	

};

int cloudfs_start(struct cloudfs_state *state,
                  const char* fuse_runtime_name) {

  int argc = 0;
  char* argv[10];
  argv[argc] = (char *) malloc(128 * sizeof(char));
  strcpy(argv[argc++], fuse_runtime_name);
  argv[argc] = (char *) malloc(1024 * sizeof(char));
  strcpy(argv[argc++], state->fuse_path);
  argv[argc++] = "-s"; // set the fuse mode to single thread
  //argv[argc++] = "-f"; // run fuse in foreground 

  state_  = *state;
	cloudfs_log_init();
  int fuse_stat = fuse_main(argc, argv, &cloudfs_operations, NULL);
  cloudfs_log_close(); 
  return fuse_stat;
}

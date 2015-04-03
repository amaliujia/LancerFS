#define _XOPEN_SOURCE 500
#include <string>
#include <cstring>
#include <utility>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <openssl/md5.h>
#include <fcntl.h>
#include <fuse.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include <time.h>
#include <unistd.h>
#include <cstring>

#include "cloudapi.h"
#include "cloudfs.h"
#include "dedup.h"
#include "cache_cloud_controller.h"


#define UNUSED __attribute__((unused))

static struct cloudfs_state state_;
cache_cloud_controller* call_controller;

void debug_msg(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
}

static int cloudfs_error(const char *error_str)
{
  int retstat = -errno;

  fprintf(stderr, "CloudFS Error: %s\n", error_str);
  return retstat;

}

/* HELPER FUNCTIONS */

/** @brief cloudBucketKey- Converts the file path to unique key used
 *                         to store in the cloud.
 *  
 *  @param filepath- Path of the file on SSD.
 *  @param hashedKey- Output is stored in this.
 */
void cloudBucketKey(const char* filepath, char* hashedKey) {
  int length=strlen(filepath);
  sprintf(hashedKey, "%s", filepath); 
  int i;
  for(i = 0; i < length ; i ++) {
    if (hashedKey[i] == '/')
      hashedKey[i] = '_';
  }
}


/** @brief cloudBucketKey- Gets the actual path of file on SSD
 * 
 *  @param filepath- Path of the file on SSD.
 *  @param path- Output is stored in this.
 */
void ssd_fullpath(char filepath[MAX_PATH_LEN], const char* path) {
  sprintf(filepath, "%s", state_.ssd_path);
  path++;
  sprintf(filepath, "%s%s", filepath, path);
}

/** @brief proxyPath- Temporary files are stored in /mnt/ssd folder
 *
 *  @param filepath- The key path
 *  @param tempFile- Generated temporary file based on filepath
 */
void proxyPath(char tempFile[MAX_PATH_LEN], const char* filepath) {

  char hashedKey[MAX_PATH_LEN] = {0};
  cloudBucketKey(filepath, hashedKey);
  sprintf(tempFile, "%s.%s", state_.ssd_path , hashedKey); 
}

void retrieve_cachePath(char* cachePath, char* tempFile) {

  char temp[MAX_PATH_LEN] = {0};
  sprintf(temp, "%s", HIDDEN_CACHE);
  sprintf(temp, "%s%s", temp, (char* )strrchr(tempFile, '/'));
  ssd_fullpath(cachePath, temp);
}



int src_to_dest(char* source, char* destination) {

  int retstat= 0;
  struct stat stat_buf;
  lstat(source, &stat_buf);

  int fd_dest = open(destination, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  int fd_src = open(source,  O_RDONLY);

  if (fd_src < 0 || fd_dest < 0)
    retstat = cloudfs_error("cloudfs_open error");

  char buf[4096];
  ssize_t read_len;
  ssize_t write_len;
  while((read_len = read(fd_src, buf, sizeof(buf))) )
    write_len = write(fd_dest, buf, read_len);

  close(fd_src);
  close(fd_dest);
  chmod(destination, stat_buf.st_mode);
  return retstat;
}

int presentInCloud(const char* fullPath) {  
  int presentCloud = 0;
  lgetxattr(fullPath, "user.presentCloud", &presentCloud, sizeof(int));
  return presentCloud;
}


/*
 * Checks if the mount points are valid or not. Initializes the FUSE file system 
 * (cloudfs) if the mount points are fine.
 *
 */
void *cloudfs_init(struct fuse_conn_info *conn UNUSED)
{
  cloud_init(state_.hostname);
  cloud_create_bucket("first");
  return NULL;
}


/*
 *  Unlink the file in cloud and it's corresponding proxy on local 
 *  if the file is in the cloud
 */
int cloudfs_unlink(const char *path) {

  char filepath[MAX_PATH_LEN] = {0};
  int retstat = 0;
  ssd_fullpath(filepath, path);
  /* If the file is in cloud, delete the object in the correspoding bucket */
  if( presentInCloud(filepath))
    call_controller->remove_from_cloud(filepath);

  retstat = unlink(filepath);
  if (retstat < 0)
    retstat = cloudfs_error("cloudfs_unlink error");
  return retstat;
}

void cloudfs_destroy(void *data UNUSED) {
  cloud_destroy();
  delete call_controller;
}

/*
 *  Read from the struct fuse_file_info to the buffer
 *  for cloud files
 *  
 */
int cloudfs_read(const char *path, char *buf, size_t size,
    off_t offset, struct fuse_file_info *fi) {

  char filepath[MAX_PATH_LEN] = {0};
  int retstat = 0;
  ssd_fullpath(filepath, path);

  if( presentInCloud(filepath)) {
    int setRead = 1;
    lsetxattr(filepath, "user.dirtybit", &setRead, sizeof(int), 0);
  }

  retstat = pread(fi->fh, buf, size, offset);
  if (retstat < 0)
    retstat = cloudfs_error("cloudfs_read error");
  return retstat;
}

/*
 *  Write the data from fuse_file_info to the buffer
 *  in order to display the content to the user.
 */
int cloudfs_write(const char *path, const char *buf, size_t size,
    off_t offset, struct fuse_file_info *fi) {
  int retstat = 0;
  char filepath[MAX_PATH_LEN] = {0};
  ssd_fullpath(filepath, path);

  /* If the file is in cloud, this is open for writing.
   * It has to be push to cloud.
   */
  if( presentInCloud(filepath)) {
    int setWrite = 2;
    lsetxattr(filepath, "user.dirtybit", &setWrite, sizeof(int), 0);
  }

  retstat = pwrite(fi->fh, buf, size, offset);
  if (retstat < 0)
    retstat = cloudfs_error("cloudfs_write error");
  return retstat;
}

/*
 *  Change the stat atrributes for cloud files
 */
void changeAttributes(struct stat *statbuf, const char* filepath, int set) {

  if( set == 1) 
    lsetxattr(filepath, "user.st_blksize", &statbuf->st_blksize, sizeof(blksize_t), XATTR_CREATE);

  int presentCloud = 1; 
  lsetxattr(filepath, "user.presentCloud", &presentCloud, sizeof(int), 0);
  lsetxattr(filepath, "user.st_size", &statbuf->st_size, sizeof(off_t), 0);
  lsetxattr(filepath, "user.st_blocks", &statbuf->st_blksize, sizeof(blkcnt_t), 0);
}

/*
 *   Populate the struct statbuf with the other attributes
 *   if the file is in cloud.
 */
int cloudfs_getattr(const char *path, struct stat *statbuf)
{
  int retstat = 0;
  char filepath[MAX_PATH_LEN] = {0};

  ssd_fullpath(filepath, path);
  if(presentInCloud(filepath)) {

    /* Filling in the statbuf */
    retstat = lstat(filepath, statbuf);
    /*
       load the stat attributes for the cloud files
       */
    lgetxattr(filepath, "user.st_size", &statbuf->st_size, sizeof(off_t));
    lgetxattr(filepath, "user.st_blksize", &statbuf->st_blksize, sizeof(blksize_t));
    lgetxattr(filepath, "user.st_blocks", &statbuf->st_blksize, sizeof(blkcnt_t));

  } else {
    retstat = lstat(filepath, statbuf);
    if (retstat < 0) {
      retstat = cloudfs_error("cloudfs_getattr error");
      return retstat;
    }
  }
  return retstat;
}

/*
 *  Obtaining the extended attribute with the given path
 */
int cloudfs_getxattr(const char *path, const char *name, 
    char *value, size_t size) {
  int retstat = 0;
  char filepath[MAX_PATH_LEN] = {0};

  ssd_fullpath(filepath, path);
  retstat = lgetxattr(filepath, name, value, size);
  if( retstat < 0){
    retstat = cloudfs_error("cloudfs_getxattr error");
  }
  return retstat;
}

/*
 *  Setting the extended attribute with the given path
 */
int cloudfs_setxattr(const char *path, const char *name, 
    const char *value, size_t size, int flags) {
  int retstat = 0;
  char filepath[MAX_PATH_LEN] = {0};

  ssd_fullpath(filepath, path);
  retstat = lsetxattr(filepath, name, value, size, flags);
  if( retstat < 0) {
    retstat = cloudfs_error("cloudfs_setxattr error");
  }
  return retstat;
}

/*
 *  Creating a directory 
 */
int cloudfs_mkdir(const char *path, mode_t mode) {

  char filepath[MAX_PATH_LEN] = {0};
  int retstat = 0;

  ssd_fullpath(filepath, path);
  retstat = mkdir(filepath, mode);
  if ( retstat < 0) {
    retstat = cloudfs_error("cloudfs_mkdir error");
  }
  return retstat;
}

/*
 *  Creating the file node.
 */
int cloudfs_mknod(const char *path, mode_t mode, dev_t dev) {
  int retstat = 0;
  char filepath[MAX_PATH_LEN] = {0};

  ssd_fullpath(filepath, path);

  if (S_ISREG(mode)) {
    retstat = open(filepath, O_CREAT | O_EXCL | O_WRONLY, mode);
    if (retstat < 0)
      retstat = cloudfs_error("cloudfs_mknod error");
    else {
      retstat = close(retstat);
      if (retstat < 0)
        retstat = cloudfs_error("cloudfs_mknod error");
    }
  } else
    if (S_ISFIFO(mode)) {
      retstat = mkfifo(filepath, mode);
      if (retstat < 0)
        retstat = cloudfs_error("cloudfs_mknod error");
    } else {
      retstat = mknod(filepath, mode, dev);
      if (retstat < 0)
        retstat = cloudfs_error("cloudfs_mknod error");
    }
  return retstat;
}

/*
 *  Opening a file: They are downloaded to temp file from cloud
 */
int cloudfs_open(const char *path, struct fuse_file_info *fi) {

  char filepath[MAX_PATH_LEN] = {0};
  char tempFile[MAX_PATH_LEN] = {0};
  int fd;
  int retstat = 0;
  ssd_fullpath(filepath, path);
  /* Temp file generated based on hashedKey */
  proxyPath(tempFile, filepath);

  if( presentInCloud(filepath) ) {
    /* Proxy file contains the segment hashes */
    call_controller->retrieve(filepath, tempFile);
  } else {
    src_to_dest(filepath, tempFile);
  }
  fd = open(tempFile, O_RDWR);
  if (fd < 0) 
    retstat = cloudfs_error("cloudfs_open open error\n");
  fi->fh = fd;
  return retstat;
}


/*
 *  SSD: Small file- close it      
 *  Large file(more than threshold): 
 *          1) Current file becomes proxy(0 size)
 *          2) "presentCloud" attribute is set
 *          3) Object pushed to cloud
 *  Files in cloud: Delete the tmporary file and upload to cloud
 */
int cloudfs_release(const char *path, struct fuse_file_info *fi) {

  int retstat = 0;
  retstat = close(fi->fh);
  char filepath[MAX_PATH_LEN] = {0};
  ssd_fullpath(filepath, path);
  char tempFile[MAX_PATH_LEN] = {0};
  proxyPath(tempFile, filepath);
  struct stat statbuf;
  lstat(tempFile, &statbuf); 

  if(presentInCloud(filepath)) {
    /* Check if the dirty bit is set or not to decide 
     * whether to go to the cache/cloud
     */
    int set;
    lgetxattr(filepath, "user.dirtybit", &set, sizeof(int));
    if( set != 1 )
      call_controller->update(filepath, tempFile);
    else
      unlink(tempFile);

    changeAttributes(&statbuf, filepath, 0);
  } 
  /* 
   *  Move the file to the cloud when it's size is more than threshold
   */ 
  else if( statbuf.st_size >= state_.threshold){

    call_controller->update(filepath, tempFile);

    changeAttributes(&statbuf, filepath, 1);
  }
  else 
    rename(tempFile, filepath);


  return retstat;
}

/*
 *   Opening the directory and storing the file descriptor  
 */
int cloudfs_opendir(const char *path, struct fuse_file_info *fi) {

  int retstat = 0;
  char filepath[MAX_PATH_LEN] = {0};
  DIR *dp;

  ssd_fullpath(filepath, path);
  dp = opendir(filepath);
  if (dp == NULL)
    retstat = cloudfs_error("cloudfs_opendir error");
  fi->fh = (intptr_t) dp;
  return retstat;
}

/*
 *   Reading the directory.
 */
int cloudfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
    struct fuse_file_info *fi) {
  int retstat = 0;
  DIR *dp;
  struct dirent *de;
  debug_msg("\ncloudfs_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n",
      path, buf, filler, offset, fi);
  dp = (DIR *)(uintptr_t) fi->fh;

  de = readdir(dp);
  if( de == 0) {
    retstat = cloudfs_error("cloudfs_readdir error");
    return retstat;
  }
  do {

    struct stat st;

    memset(&st, 0, sizeof(st));
    st.st_ino = de->d_ino;
    st.st_mode = de->d_type << 12;
    if ( filler(buf, de->d_name, &st, 0) != 0) {

      return -ENOMEM;
    }
  } while( (de = readdir(dp)) != NULL);
  return retstat;
}

/*
 *   Checking the file access permission
 */
int cloudfs_access(const char *path, int mask) {

  char filepath[PATH_MAX]= {0};
  int retstat = 0;

  ssd_fullpath(filepath, path);
  retstat = access(filepath, mask);

  if (retstat < 0)
    retstat = cloudfs_error("cloudfs_access error");
  return retstat;
}

/*
 *   Setting the time of directory/file
 */
int cloudfs_utimens(const char* path, const struct timespec ts[2]) {
  char filepath[PATH_MAX]= {0};
  int retstat = 0;
  ssd_fullpath(filepath, path);
  retstat = utimensat(0, filepath, ts, AT_SYMLINK_NOFOLLOW);
  if( retstat < 0) 
    retstat = cloudfs_error("cloudfs_utimens error");
  return retstat;
}

/*
 *   Changing the permission level of file
 */
int cloudfs_chmod(const char *path, mode_t mode) {
  int retstat = 0;
  char filepath[PATH_MAX] = {0};

  ssd_fullpath(filepath, path);

  retstat = chmod(filepath, mode);
  if (retstat < 0)
    retstat = cloudfs_error("cloudfs_chmod chmod");
  return retstat;
}



/*
 *   Removing an empty directory
 */
int cloudfs_rmdir(const char *path) {
  int retstat = 0;
  char filepath[PATH_MAX] = {0};

  ssd_fullpath(filepath, path);

  retstat = rmdir(filepath);
  if (retstat < 0)
    retstat = cloudfs_error("cloudfs_rmdir rmdir");

  return retstat;
}

/*
 * Functions supported by cloudfs 
 */
static 
struct fuse_operations cloudfs_operations; 

int cloudfs_start(struct cloudfs_state *state,
    const char* fuse_runtime_name) {

  cloudfs_operations.init           = cloudfs_init;
  cloudfs_operations.getattr        = cloudfs_getattr;
  cloudfs_operations.mkdir          = cloudfs_mkdir;
  cloudfs_operations.readdir        = cloudfs_readdir;
  cloudfs_operations.destroy        = cloudfs_destroy;
  cloudfs_operations.getxattr       = cloudfs_getxattr;
  cloudfs_operations.setxattr       = cloudfs_setxattr;
  cloudfs_operations.mknod          = cloudfs_mknod;
  cloudfs_operations.open           = cloudfs_open;
  cloudfs_operations.read           = cloudfs_read;
  cloudfs_operations.write          = cloudfs_write;
  cloudfs_operations.release        = cloudfs_release;
  cloudfs_operations.opendir        = cloudfs_opendir;
  cloudfs_operations.access         = cloudfs_access;
  cloudfs_operations.utimens        = cloudfs_utimens;
  cloudfs_operations.chmod          = cloudfs_chmod;
  cloudfs_operations.unlink         = cloudfs_unlink;
  cloudfs_operations.rmdir          = cloudfs_rmdir;

  int argc = 0;
  char* argv[10];
  argv[argc] = (char *) malloc(128 * sizeof(char));
  strcpy(argv[argc++], fuse_runtime_name);
  argv[argc] = (char *) malloc(1024 * sizeof(char));
  strcpy(argv[argc++], state->fuse_path);
  argv[argc++] =(char*)&"-s"; // set the fuse mode to single thread
  //argv[argc++] = "-f"; // run fuse in foreground 
  state_  = *state;

  call_controller = new cache_cloud_controller(&state_);
  int fuse_stat = fuse_main(argc, argv, &cloudfs_operations, NULL);
  return fuse_stat;
}

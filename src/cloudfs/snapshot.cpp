#include "snapshot.h"

#define INDEX_CHUNK "/.index.snapshot"
#define SSD_DATA_PATH "data"

SnapshotManager::SnapshotManager(const char *s){
	 strcpy(ssd_path, s);
}

SnapshotManager::~SnapshotManager(){
	
}

TIMESTAMP SnapshotManager::deletes(TIMESTAMP t){
    for(size_t i = 0; i < records.size(); i++){
        if(records[i] == t){
            records.erase(records.begin() + i);
        }
    }
}

TIMESTAMP *SnapshotManager::list(){
    TIMESTAMP *t = &records[0];
    return t;
}

TIMESTAMP SnapshotManager::snapshot(){
	struct timeval tv;
	gettimeofday(&tv, NULL);
	TIMESTAMP t = (TIMESTAMP)tv.tv_sec;			

  char tarFilename[MAX_PATH_LEN];
  sprintf(tarFilename, "%s", ssd_path);
  sprintf(tarFilename, "%s%lu", tarFilename, t); 
			
	tar(tarFilename);	

  char cloudpath[MAX_PATH_LEN];
	sprintf(cloudpath, "%lu", t);
	
	push_to_cloud(cloudpath, tarFilename);
	unlink(tarFilename);

	//save snapshot	record into index
	records.push_back(t);

	//write to disk
	serialization();
			
	return t;
}

/*
    Save snapshot historical records into disk.
 */
void SnapshotManager::serialization(){
	char fpath[MAX_PATH_LEN];
	sprintf(fpath, "%s%s", ssd_path, SSD_DATA_PATH);
	sprintf(fpath, "%s%s", fpath,	INDEX_CHUNK); 		
	FILE *fp = fopen(fpath, "w");

	fprintf(fp, "%d\n", records.size());
	for(size_t i = 0; i < records.size(); i++){
		fprintf(fp, "%lu\n", records[i]);
	}

	fclose(fp);		
	push_to_cloud("record", fpath);
}

/*
    Function provided when recursively delete a directory.
 */
static int tree_delete(const char *fpath, const struct stat *sb,
                        int tflag, struct FTW *ftwbuf)
{
    switch (tflag) {
        case FTW_D:
        case FTW_DNR:
        case FTW_DP:
            rmdir (fpath);
            break;
        default:
            unlink (fpath);
            break;
    }
    return (0);
}

/*
    Recovery snapshot historical records.
 */
void SnapshotManager::recover_index(TIMESTAMP t){
    char fpath[MAX_PATH_LEN];
    sprintf(fpath, "%s%s", ssd_path, SSD_DATA_PATH);
    sprintf(fpath, "%s%s", fpath, INDEX_CHUNK);
    
    //load log into disk.
    FILE *fp = fopen(fpath, "r");
    records.clear();
    if(fp == NULL){
        return;
    }
    int ret;
    int size;
    ret = fscanf(fp, "%d", &size);
    if(ret != 1){
        fclose(fp);
        return;
    }
    
    int i;
    for(i = 0; i < size; i++){
        TIMESTAMP ts;
        ret = fscanf(fp, "%lu", &ts);
        if(ret != 1){
            fclose(fp);
            return;
        }
        records.push_back(ts);
        if(ts == t){
            break;
        }
    }
    
    char cloudpath[MAX_PATH_LEN];
    
    for(i = i + 1; i < size; i++){
        TIMESTAMP ts;
        ret = fscanf(fp, "%lu", &ts);
        if(ret != 1){
            fclose(fp);
            return;
        }
        
        //delete snapshot
        sprintf(cloudpath, "%lu", ts);	
        delete_object("snapshot", cloudpath);
    }
    
    serialization();
}

void SnapshotManager::restore(TIMESTAMP t){
    log_msg("\nSnapshotManager::restore %lu\n", t);
    char root[MAX_PATH_LEN];
    sprintf(root, "%s%s", ssd_path, SSD_DATA_PATH);
    nftw(root, tree_delete, 20, FTW_DEPTH);
    mkdir(root, S_IRWXU | S_IRWXG | S_IRWXO);
    
    char cloudpath[MAX_PATH_LEN];
    sprintf(cloudpath, "%lu", t);
    
    char tarFilename[MAX_PATH_LEN];
    sprintf(tarFilename, "%s", ssd_path);
    sprintf(tarFilename, "%s%lu", tarFilename, t);
    
    get_from_cloud("snapshot", cloudpath, tarFilename);
    
    untar(tarFilename);
    
    char fpath[MAX_PATH_LEN];
    sprintf(fpath, "%s%s", ssd_path, SSD_DATA_PATH);
    sprintf(fpath, "%s%s", fpath, INDEX_CHUNK);
    get_from_cloud("snapshot", "record", fpath);
    
    recover_index(t);
				
    return;
}

void SnapshotManager::log_msg(const char *format, ...){
    va_list ap;
    va_start(ap, format);
    vfprintf(logfd, format, ap);
    fflush(logfd);
}

/*
    Helper function. Used to tar fuse namespace.
 */
void SnapshotManager::tar(const char *tarFilename){
    TAR *pTar;
    
    char fpath[MAX_PATH_LEN];
    sprintf(fpath, "%sdata", ssd_path);
    
    tar_open(&pTar, tarFilename, NULL, O_WRONLY | O_CREAT, 0644, TAR_GNU);
    tar_append_tree(pTar, fpath, ".");
    close(tar_fd(pTar));
}

/*
    Helper function. Used to untar into fuse namespace.
 */
void SnapshotManager::untar(const char *tarFilename){
    TAR *pTar;
    
    char fpath[MAX_PATH_LEN];
    sprintf(fpath, "%sdata", ssd_path);
    
    tar_open(&pTar, tarFilename, NULL, O_RDONLY, 0, TAR_GNU);
    tar_extract_all(pTar, fpath);
    tar_close(pTar);	
}


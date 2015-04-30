#include "snapshot.h"

SnapshotManager::SnapshotManager(){
}

SnapshotManager::~SnapshotManager(){
	
}

void SnapshotManager::recovery(){
	return;
}

TIMESTAMP SnapshotManager::deletes(TIMESTAMP t){
	return 0;
}

TIMESTAMP *SnapshotManager::list(){
	return NULL;
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
	return t;
}

void SnapshotManager::restore(TIMESTAMP t){
  char cloudpath[MAX_PATH_LEN];
  sprintf(cloudpath, "%lu", t);
 
	char tarFilename[MAX_PATH_LEN];
  sprintf(tarFilename, "%s", ssd_path);
  sprintf(tarFilename, "%s%lu", tarFilename, t);			

	get_from_cloud("snapshot", cloudpath, tarFilename);

	untar(tarFilename);
	unlink(tarFilename);			
	return;
}

void SnapshotManager::tar(const char *tarFilename){
	TAR *pTar; 	

  char fpath[MAX_PATH_LEN];
  sprintf(fpath, "%sdata", ssd_path);

	tar_open(&pTar, tarFilename, NULL, O_WRONLY | O_CREAT, 0644, TAR_GNU);
  tar_append_tree(pTar, ssd_path, ".");
  close(tar_fd(pTar));
}

void SnapshotManager::untar(const char *tarFilename){
	TAR *pTar;

	tar_open(&pTar, tarFilename, NULL, O_RDONLY, 0, TAR_GNU);
	tar_extract_all(pTar, ssd_path);	
	tar_close(pTar);	
}


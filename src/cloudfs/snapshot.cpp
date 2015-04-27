#include "snapshot.h"

SnapshotManager::SnapshotManager(){
	cloud_create_bucket("snapshot");
}

SnapshotManager::~SnapshotManager(){
	
}

void SnapshotManager::recovery(){

}

TIMESTAMP SnapshotManager::deletes(TIMESTAMP t){

}

TIMESTAMP *SnapshotManager::list(){

}

void SnapshotManager::snapshot(TIMESTAMP t){

}

void SnapshotManager::restore(TIMESTAMP t){

}

void SnapshotManager::tar(TIMESTAMP t){
	TAR *pTar; 	

	char tarFilename[MAX_PATH_LEN];
	sprintf(tarFilename, "%s", ssd_path);
  sprintf(tarFilename, "%s%lu", tarFilename, t); 
	
	tar_open(&pTar, tarFilename, NULL, O_WRONLY | O_CREAT, 0644, TAR_GNU);
  tar_append_tree(pTar, srcDir, extractTo);
  close(tar_fd(pTar));
}

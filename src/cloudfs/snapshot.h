#ifndef SNAPSHOT_H__
#define SNAPSHOT_H__


//c++
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <string>

#include "transmission.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "cloudapi.h"

#ifdef __cplusplus
}
#endif

#include <libtar.h>
#include "Fuse.h"

using namespace std;
 
class SnapshotManager{
public:
  char ssd_path[MAX_PATH_LEN];
  char fuse_path[MAX_PATH_LEN];	
	vector<TIMESTAMP> records;

private:
	void tar(TIMESTAMP t);
	void recovery();	

public:
	SnapshotManager();
	~SnapshotManager();

	void snapshot(TIMESTAMP t);
	void restore(TIMESTAMP  t);
	TIMESTAMP *list();
	TIMESTAMP deletes(TIMESTAMP t); 		
};


#endif //SNAPSHOT_H__

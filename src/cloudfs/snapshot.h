#ifndef SNAPSHOT_H__
#define SNAPSHOT_H__



//c++
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <string>

#ifdef __cplusplus
extern "C"
{
#endif

#include "cloudapi.h"

#ifdef __cplusplus
}
#endif

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <stdarg.h>
#include <sys/time.h>
#include <libtar.h>
#include <ftw.h>

#include "Fuse.h"
#include "log.h"
#include "transmission.h"

using namespace std;
 
class SnapshotManager{
public:
  char ssd_path[MAX_PATH_LEN];
  char fuse_path[MAX_PATH_LEN];	
	FILE *snapfd;
	vector<TIMESTAMP> records;

private:
	void tar(const char *tarFilename);
	void untar(const char *tarFIlename);
	void serialization();
	void recover_index(TIMESTAMP t);

public:
	SnapshotManager(const char *s);
	~SnapshotManager();

	TIMESTAMP snapshot();
	void restore(TIMESTAMP  t);
	TIMESTAMP *list();
	TIMESTAMP deletes(TIMESTAMP t); 		
};


#endif //SNAPSHOT_H__

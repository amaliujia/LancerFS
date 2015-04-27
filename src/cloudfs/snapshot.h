#ifndef SNAPSHOT_H__
#define SNAPSHOT_H__


#ifdef __cplusplus
extern "C"
{
#endif

#include <archive.h>

#ifdef __cplusplus
}
#endif

#include <libtar.h>
#include "Fuse.h"
 
class SnapshotManager{
private:
	
public:
	SnapshotManager();
	~SnapshotManager();

	void snapshot();
	void restore();
	unsigned long *list();
	unsigned long deletes(); 		
};


#endif //SNAPSHOT_H__

#include <stdio.h>
#include <stdlib.h>
#include <openssl/md5.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <cstring>

/*C++ Includes */
#include <map>
#include <string>
#include <cstring>
#include <utility>
#include <vector>

int deduplication_layer:: infile;
int deduplication_layer:: segmentBegin;
int deduplication_layer:: outfile;
int deduplication_layer:: bytes_remaining;
char* deduplication_layer:: readSegment;
struct cloudfs_state* deduplication_layer:: fs_state;

#include "cache_cloud_controller.h"
#include "cloudfs.h"
#include "cloudapi.h"
#include "dedup.h"
#include "cache_object_item.h"

#include "deduplication_layer.h"

using namespace std;

/**
 * deduplication_layer: Constructor to initialize the cloud
 */
deduplication_layer::deduplication_layer(struct cloudfs_state* state_) {
  fs_state = state_;
  min_seg_size = fs_state->avg_seg_size / 4;
  max_seg_size = fs_state->avg_seg_size * 4;
  readSegment = (char*)malloc(max_seg_size);
  build_hashTable();
}


/**
 * hashTable_to_SSD: Construct the hash table containing the mapping
 *                    from segments to the reference counts. 
 */
void deduplication_layer::hashTable_to_SSD() {

  char filepath[MAX_PATH_LEN] = {0};
  ssd_fullpath(filepath, HASH_MAP_LOCATION);
  FILE* f = fopen(filepath, "w");
  map<string, int>:: iterator it;
  for(it = hashTableMap.begin(); it != hashTableMap.end(); it++ ) {
    fprintf(f, "%s %d\n", (it->first).c_str(), it->second);
  }
  fclose(f);
}


/**
 * build_hashTable: Construct the hash table containing the mapping
 *                    from segments to the reference counts. 
 */
void deduplication_layer::build_hashTable() {

  char filepath[MAX_PATH_LEN] ={0};
  ssd_fullpath(filepath, HASH_MAP_LOCATION);
  FILE* fp = fopen(filepath, "r");

  /* The backup is on SSD */
  if (fp != NULL) {
    int ref;
    char hashValue[MD5_DIGEST_LENGTH * 2 + 1] = {0};

    while(fscanf(fp, "%s %d", hashValue, &ref) == 2)
      hashTableMap[hashValue] = ref;

    fclose(fp);
  }
}


/**
 * get_buffer: Get object from cloud 
 */
int deduplication_layer::get_buffer(const char *buffer, int bufferLength) {
  return write(outfile, buffer, bufferLength);  
}

/**
 * put_buffer: Put object to cloud 
 */
int deduplication_layer::put_buffer(char *buffer, int bufferLength){

  if(fs_state->no_dedup) {
    return read(infile, buffer, bufferLength);
  } else {
    if( bytes_remaining == 0 )
      return 0;

    if( bytes_remaining >= bufferLength) {
      memcpy(buffer, readSegment, bufferLength);
      bytes_remaining -= bufferLength;
      return bufferLength;
    } 
    else {
      int tmp = bytes_remaining;
      bytes_remaining = 0;
      memcpy(buffer, readSegment, tmp);
      return tmp;
    }
  }

}



/**
 * remove_from_cloud: Deletes object from the cloud
 */
void deduplication_layer::remove_from_cloud(char* filepath) {
  char keyPath[MAX_PATH_LEN] = {0};
  cloudBucketKey(filepath, keyPath);
  if( fs_state->no_dedup ) {
    cloud_delete_object("first", keyPath);
    cloud_print_error();
  } else {
    char hashValue[MD5_DIGEST_LENGTH * 2 + 1] = {0};

    std:: map<std::string, int>:: iterator it;
    FILE* proxy_file = fopen(filepath, "r");
    while(fscanf(proxy_file, "%s", hashValue) == 1) {

      it = hashTableMap.find(hashValue);
      it->second --;
      if(it->second == 0) {
        hashTableMap.erase(hashValue);
        cloud_delete_object("first", hashValue);
        cloud_print_error();  
      }
    }
    hashTable_to_SSD();
  }
}

/**
 * pull_from_cloud: Downloads files from cloud to the proxy
 */
void deduplication_layer::pull_from_cloud(char* filepath, char* tempFile) {

  outfile = open(tempFile, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  if( fs_state->no_dedup  ) {
    char keyPath[MAX_PATH_LEN] = {0}; 
    cloudBucketKey(filepath, keyPath);

    cloud_get_object("first", keyPath, get_buffer);
    cloud_print_error();
  } else {
    pathTohashMap[filepath].clear(); 
    char hashValue[MD5_DIGEST_LENGTH * 2 + 1] = {0};

    FILE* proxy_file = fopen(filepath, "r");
    while(fscanf(proxy_file, "%s", hashValue) == 1) {
      cloud_get_object("first", hashValue, get_buffer);
      cloud_print_error();

      if( pathTohashMap[filepath].find(hashValue) == pathTohashMap[filepath].end()) {
        pathTohashMap[filepath][hashValue] = 1;
      } else {
        pathTohashMap[filepath][hashValue] += 1;
      }
    }
    fclose(proxy_file);
  }
  close(outfile);
}




/**
 * build_pathTohashMap: Building the hash Map
 */
void deduplication_layer::build_pathTohashMap(char* filepath) {
  if( !fs_state->no_dedup  ) {
    pathTohashMap[filepath].clear(); 
    char hashValue[MD5_DIGEST_LENGTH * 2 + 1] = {0};
    FILE* proxy_file = fopen(filepath, "r");
    while(fscanf(proxy_file, "%s", hashValue) == 1) {

      if( pathTohashMap[filepath].find(hashValue) == pathTohashMap[filepath].end())
        pathTohashMap[filepath][hashValue] = 1;
      else
        pathTohashMap[filepath][hashValue] += 1; 
    }
    fclose(proxy_file);
  }
}

/**
 * upload_to_cache: Put the file in cache
 */
void deduplication_layer::upload_to_cache(char* filepath, char* tempFile) {
  char cachePath[MAX_PATH_LEN] = {0};
  retrieve_cachePath(cachePath, tempFile);
  src_to_dest(tempFile, cachePath);

  if( !presentInCloud( filepath) ) {
    FILE* file = fopen(tempFile, "w");
    fclose(file);
    rename(tempFile, filepath);
  } else 
    unlink( tempFile );
}

/**
 * putSegmentsInCloud: Upload the segmented file into cloud
 *
 */

void deduplication_layer::check_hashtable_entry(char *hashValue, int segment_len, int *retstat, char *filepath) {
  /* Hash incurred first time  */
  if(hashTableMap.find(hashValue) == hashTableMap.end()) {
    hashTableMap[hashValue] = 1;
    *retstat = pread(infile, readSegment, segment_len, segmentBegin);
    bytes_remaining = segment_len;
    cloud_put_object("first",hashValue, segment_len, put_buffer);
  } else {
    /* Already encountered this hash */
    if ((pathTohashMap.find(filepath) != pathTohashMap.end()) && pathTohashMap[filepath].find(hashValue) != pathTohashMap[filepath].end())
      pathTohashMap[filepath][hashValue]--;
    else
      hashTableMap[hashValue]++;
  }
}

void deduplication_layer::putSegmentsInCloud(char* filepath, char* tempFile) {
  infile = open(tempFile, O_RDONLY);        
  if ( !fs_state->no_dedup ) {
    rabinpoly_t *rp = rabin_init( fs_state->rabin_window_size, fs_state->avg_seg_size, 
        min_seg_size , max_seg_size);
    MD5_CTX ctx;
    std:: vector<std::string> listOfSegments;
    unsigned char md5[MD5_DIGEST_LENGTH];  
    char buf[1024];	 
    char hashValue[MD5_DIGEST_LENGTH * 2 + 1] = {0};
    int new_segment = 0;
    int len, segment_len = 0, b;
    int retstat, bytes;
    segmentBegin = 0;

    MD5_Init(&ctx);
    while( (bytes = read(infile, buf, sizeof buf)) > 0 ) {
      char *buftoread = (char *)&buf[0];
      while ((len = rabin_segment_next(rp, buftoread, bytes, 
              &new_segment)) > 0) {
        MD5_Update(&ctx, buftoread, len);
        segment_len += len;
        if (new_segment) {
          MD5_Final(md5, &ctx);

          for(b = 0; b < MD5_DIGEST_LENGTH; b++)
            sprintf(&hashValue[b*2], "%02x", md5[b]);

          listOfSegments.push_back(hashValue);
          check_hashtable_entry(hashValue, segment_len, &retstat, filepath);
          MD5_Init(&ctx);
          segmentBegin += segment_len;
          segment_len = 0;
        }
        buftoread += len;
        bytes -= len;
        if (!bytes) {
          break;
        }
      }
      if (len == -1) {
        fprintf(stderr, "Segment not processed\n");
        exit(2);
      }
    }
    MD5_Final(md5, &ctx);

    for(b = 0; b < MD5_DIGEST_LENGTH; b++) {
      sprintf(&hashValue[b*2], "%02x", md5[b]);
      debug_msg("%02x", md5[b]);
    }

    listOfSegments.push_back(hashValue);
    check_hashtable_entry(hashValue, segment_len, &retstat, filepath);
    close(infile);

    /* To be clear about the reference count, difference mapping is done */
    if(pathTohashMap.find(filepath) != pathTohashMap.end()) {
      std::map<std::string, int>::iterator it;
      for(it = pathTohashMap[filepath].begin(); it != pathTohashMap[filepath].end(); it++) {
        debug_msg("\nLEFT %s %d\n", (it->first).c_str(), it->second);
        hashTableMap[it->first] -= it->second;
        if( hashTableMap[it->first] == 0) {
          cloud_delete_object("first", (it->first).c_str());
          hashTableMap.erase(it->first);
        }
      }
    }

    std::vector<std::string>::iterator it;
    // clear the file, and add the segments list
    FILE* file = fopen(tempFile, "w");
    for(it = listOfSegments.begin(); it != listOfSegments.end(); it++) {
      fprintf(file, "%s\n", (*it).c_str());
    }
    hashTable_to_SSD();
    fclose(file);
  } else {

    struct stat statbuf;
    char keyPath[MAX_PATH_LEN];
    cloudBucketKey(filepath, keyPath);

    lstat(tempFile, &statbuf);  
    cloud_put_object("first", keyPath, statbuf.st_size, put_buffer);
    cloud_print_error();
    close(infile);
    truncate(filepath,0);
  }

}

deduplication_layer::~deduplication_layer(){
  free(readSegment);
}

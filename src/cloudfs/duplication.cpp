#define INDEX_FILE "/.index.file"
#define INDEX_CHUNK "/.index.chunk"

#include "duplication.h"
#define UNUSED __attribute__((unused))
static  FILE *outfile;
static  FILE *infile;

int get_buffer(const char *buffer, int bufferLength) {
  return fwrite(buffer, 1, bufferLength, outfile);
}

int put_buffer(char *buffer, int bufferLength) {
  return fread(buffer, 1, bufferLength, infile);
}

void duplication::cloud_get_shadow(const char *fullpath, const char *cloudpath){
	outfile = fopen(fullpath, "wb");
  cloud_get_object("bkt", cloudpath, get_buffer);
	fclose(outfile);
} 

void duplication::cloud_push_file(const char *fpath, struct stat *stat_buf){
	lstat(fpath, stat_buf);
	
  char cloudpath[MAX_PATH_LEN];
  memset(cloudpath, 0, MAX_PATH_LEN);
  strcpy(cloudpath, fpath);
  cloud_filename(cloudpath);
	
	infile = fopen(fpath, "rb");
	if(infile == NULL){
			log_msg("LancerFS error: cloud push %s failed\n", fpath);
			return;		
	}
	log_msg("LancerFS log: cloud_push_file(path=%s)\n", fpath);
  cloud_put_object("bkt", cloudpath, stat_buf->st_size, put_buffer);
  fclose(infile);	
}

void duplication::cloud_filename(char *path){
	while(*path != '\0'){
      if(*path == '/'){
          *path = '+';
      }
      path++;
  }
}

void duplication::cloud_push_shadow(const char *fullpath){
	struct stat stat_buf;
	
	char cloudpath[MAX_PATH_LEN+3];
	memset(cloudpath, 0, MAX_PATH_LEN+3);
	strcpy(cloudpath, fullpath);
	
	infile = fopen(fullpath, "rb");
  if(infile == NULL){
     log_msg("LancerFS error: cloud push %s failed, cloudpath %s\n", fullpath, cloudpath);
      return;
  }	
	
	cloud_filename(cloudpath);
 	log_msg("LancerFS log: cloud_push_file(path=%s)\n", cloudpath);
  lstat(fullpath, &stat_buf);
  cloud_put_object("bkt", cloudpath, stat_buf.st_size, put_buffer);
  fclose(infile);
}

void duplication::ssd_fullpath(const char *path, char *fpath){
  sprintf(fpath, "%s", state_.ssd_path);
  path++;
  sprintf(fpath, "%s%s", fpath, path);	
}

duplication::duplication(FILE *fd, char *ssd_path UNUSED){
  window_size = 48;
  avg_seg_size = 4096;
  min_seg_size = 2048;
  max_seg_size = 8192;
	logfd = fd;

	//init rabin 
	init_rabin_structrue();

	//recover index
	recovery();
}

duplication::duplication(FILE *fd, fuse_struct *state){
  state_.copy(state);
	window_size = state_.rabin_window_size;
  avg_seg_size = state_.avg_seg_size;
  min_seg_size = 0.5 * state_.avg_seg_size;
  max_seg_size = 2 * state_.avg_seg_size;
	state_.no_dedup = state_.no_dedup ^ 1;  
	logfd = fd;

  //init rabin 
  init_rabin_structrue();

  //recover index
  if(state_.no_dedup)
		recovery();
}

void duplication::log_msg(const char *format, ...){
    va_list ap;
    va_start(ap, format);
    vfprintf(logfd, format, ap);
    fflush(logfd);
}

void duplication::init_rabin_structrue(){
	//init rabin 
	rp = rabin_init(window_size, avg_seg_size, min_seg_size, max_seg_size);
	if (!rp) {
    log_msg("Failed to init rabinhash algorithm\n");
		exit(1);
  }
}

void duplication::deduplicate(const char *path){
		if(state_.no_dedup){
			log_msg("deduplicate(path=%s)\n", path);
			std::string s(path);

			//get fingerprint	
			vector<MD5_code> code_list;
			fingerprint(path, code_list);

			//if file exist
			map<string, vector<MD5_code> >::iterator iter;
			iter = file_map.find(s);
			if(iter == file_map.end()){
				file_map.insert(pair<string, vector<MD5_code> >(s, code_list));	
			}

			//update chunk
			update_chunk(path, code_list);											

			//maintain indisk index
			serialization();
		}else{
			//push to cloud
      struct stat stat_buf;
      lstat(path, &stat_buf);
			cloud_push_file(path, &stat_buf);	
		}	
}

void duplication::update_chunk(const char *fpath, vector<MD5_code> &code_list){
	map<MD5_code, int>::iterator iter;
	long offset = 0;
	for(unsigned int i = 0; i < code_list.size(); i++){
		MD5_code c = code_list[i];	
		//bug here !!! wrong compare, I guess
		if((iter = chunk_set.find(c)) != chunk_set.end()){
			iter->second += 1;	
		}else{
			chunk_set.insert(pair<MD5_code, int>(c, 1));	
			//push to cloud
			put(fpath, c, offset);		
		}
		offset += c.segment_len;								
	}				
}

void duplication::put(const char *fpath, MD5_code &code, long offset){
	infile = fopen(fpath, "rb");

  if(infile == NULL){
     log_msg("LancerFS error: cloud push %s\n", fpath);
     return;
  }	

	int ret = fseek(infile, offset, SEEK_SET);							
	if(ret != 0){
			log_msg("LancerFS error: cloud push %s offset %l\n", fpath, offset); 
      return;
	}
	
	cloud_put_object("bkt", code.md5, code.segment_len, put_buffer);
	fclose(infile);				
}

duplication::~duplication(){
	rabin_free(&rp);
}

void duplication::clean(const char *fpath){
		if(state_.no_dedup){
			log_msg("clean(path=%s\n", fpath);
			remove(fpath);
			deduplicate(fpath);
		}else{
			cloud_push_shadow(fpath);	
		}
}

void duplication::serialization(){
	char fpath[PATH_LEN];
	ssd_fullpath(INDEX_FILE, fpath);
	FILE *fp = fopen(fpath, "w");
	fprintf(fp, "%d\n", file_map.size());

	map<string, vector<MD5_code> >::iterator iter;		
	for(iter = file_map.begin(); iter != file_map.end(); iter++){
		fprintf(fp, "%s %d\n", iter->first.c_str(), iter->second.size());
		for(size_t j = 0; j < iter->second.size(); j++){
			fprintf(fp, "%s\n",iter->second[j].md5);		
		}	
	}
	fclose(fp);

	ssd_fullpath(INDEX_CHUNK, fpath);
	fp = fopen(fpath, "w");	
	map<MD5_code, int>::iterator iter2;
	for(iter2 = chunk_set.begin(); iter2 != chunk_set.end(); iter2++){
		fprintf(fp, "%s %d\n", iter2->first.md5, iter2->second);	
	}
	fclose(fp);

	log_msg("LancerFS log: current index: files %d chunks %d\n", file_map.size(), chunk_set.size());
}

void duplication::recovery(){
  char fpath[PATH_LEN];
  ssd_fullpath(INDEX_FILE, fpath);
  FILE *fp = fopen(fpath, "r");
	
	if(fp != NULL){
		int file_num = 0;
		int ret = 0;
		ret = fscanf(fp, "%d", &file_num);
		if(ret != 1){
			log_msg("no data in index\n");
			fclose(fp);
			return;
		}
		for(int i = 0; i < file_num; i++){
			char filepath[PATH_LEN];
			int md5_num = 0;	
			ret = fscanf(fp, "%s %d", filepath, &md5_num);
			if(ret != 2){
				log_msg("wrong md5 num %s %d\n", filepath, md5_num);
				continue;
			}
			vector<MD5_code> chunks;
			for(int j = 0; j < md5_num; j++){
					char md5[MD5_LEN] = {0};	
					ret = fscanf(fp, "%s", md5);
      		if(ret != 1){
        		log_msg("wrong md5 %s\n", filepath);
						continue;
      		}
					MD5_code c;
					//TODO:: no len here
					c.set_code(md5);
					chunks.push_back(c);			
			}
			string s(filepath);
			file_map.insert(pair<string, vector<MD5_code> >(s, chunks));
		}	 	
	}else{
		return;
	}
	fclose(fp);
	
	//read md5 chunk
  ssd_fullpath(INDEX_CHUNK, fpath);
 	fp = fopen(fpath, "r");
	char md5[MD5_LEN] = {0};
	int count = 0;	
	while(fscanf(fp,"%s %d", md5, &count) == 2){
		MD5_code c;
		c.set_code(md5);
		chunk_set.insert(pair<MD5_code, int>(c, count));	
	} 
	fclose(fp);	
}

void duplication::remove(const char *fpath){
	if(state_.no_dedup){
		log_msg("remove(path=%s\n", fpath);
		string s(fpath);
		map<string, vector<MD5_code> >::iterator iter;
		if((iter = file_map.find(s)) == file_map.end()){
			log_msg("LancerFS error: %s doesn't exist in index\n", fpath);
			return;
		}

		vector<MD5_code> chunks = file_map[s];
		map<MD5_code, int>::iterator chunk_iter;	
		for(unsigned int i = 0; i < chunks.size(); i++){
			MD5_code c = chunks[i];

			if((chunk_iter = chunk_set.find(c)) != chunk_set.end()){
				if(chunk_iter->second == 1){
					cloud_delete_object("bkt", c.md5);	
					chunk_set.erase(chunk_iter);
				}else{
					chunk_iter->second -= 1;
				}		
			}	
		}

		file_map.erase(iter);
		//maintain indisk index
		serialization();
	}else{
			char cloudpath[MAX_PATH_LEN];
			memset(cloudpath, 0, MAX_PATH_LEN);
			strcpy(cloudpath, fpath);
			cloud_filename(cloudpath);	
			cloud_delete_object("bkt", cloudpath);
	} 
}

void duplication::retrieve(const char *fpath){
	if(state_.no_dedup){
		log_msg("retrieve(path=%s\n", fpath);
		string s(fpath);
		map<string, vector<MD5_code> >::iterator iter;
		if((iter = file_map.find(s)) == file_map.end()){
			log_msg("LancerFS error: %s doesn't exist in index, size %d\n", fpath, file_map.size());
			return;	
		}			

		vector<MD5_code> chunks = file_map[s];
		long offset = 0;
		for(unsigned int i = 0; i < chunks.size(); i++){
			MD5_code c = chunks[i];
			get(fpath, c, offset);
			offset += c.segment_len;	
		}
	}else{
			log_msg("LancerFS log: open proxy file %s\n", fpath);
		  char cloudpath[MAX_PATH_LEN];
  		memset(cloudpath, 0, MAX_PATH_LEN);
  		strcpy(cloudpath, fpath);	
			cloud_filename(cloudpath);
			
			cloud_get_shadow(fpath, cloudpath);
				
			int dirty = 0;
			lsetxattr(fpath, "user.dirty", &dirty, sizeof(int), 0);		
	}			
}

void duplication::get(const char *fpath, MD5_code &code, long offset){
  outfile = fopen(fpath, "ab");
	 
	if(outfile == NULL){
     log_msg("LancerFS error: cloud pull %s\n", fpath);
     return;
  }

	int ret = fseek(outfile, offset, SEEK_SET);
  if(ret != 0){
      log_msg("LancerFS error: cloud pull %s offset %l\n", fpath, offset);
      return;
  }

	cloud_get_object("bkt", code.md5, get_buffer);
  fclose(outfile);
}
void duplication::fingerprint(const char *path, vector<MD5_code> &code_list){
	int	fd = open(path, O_RDONLY);
  
	MD5_CTX ctx;
  unsigned char md5[MD5_DIGEST_LENGTH];
  int new_segment = 0;
  int len, segment_len = 0, b UNUSED; 
  char buf[1024];
  int bytes;

  MD5_Init(&ctx);
  while((bytes = read(fd, buf, sizeof buf)) > 0){
    char *buftoread = (char *)&buf[0];
    while ((len = rabin_segment_next(rp, buftoread, bytes,
                      &new_segment)) > 0) {
      MD5_Update(&ctx, buftoread, len);
      segment_len += len;

      if (new_segment) {
        MD5_Final(md5, &ctx);
				MD5_code code;
				code.set_code(md5, segment_len);
				code_list.push_back(code);
        MD5_Init(&ctx);
        segment_len = 0;
      }

      buftoread += len;
      bytes -= len;

      if(!bytes){
        break;
      }
    }
    if(len == -1){
			log_msg("Failed to process the segment\n");
			exit(2);
    }
  }
  MD5_Final(md5, &ctx);
  MD5_code code;
  code.set_code(md5, segment_len);
  code_list.push_back(code);
	
	rabin_reset(rp);
}

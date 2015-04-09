#include "duplication.h"

static  FILE *outfile;
static  FILE *infile;

int get_buffer(const char *buffer, int bufferLength) {
  return fwrite(buffer, 1, bufferLength, outfile);
}

int put_buffer(char *buffer, int bufferLength) {
  return fread(buffer, 1, bufferLength, infile);
}

duplication::duplication(FILE *fd){
  window_size = 48;
  avg_seg_size = 4096;
  min_seg_size = 2048;
  max_seg_size = 8192;
  //fname[PATH_MAX] = {0};
	memset(fname, 0, PATH_MAX);
	logfd = fd;

	//init rabin 
	init_rabin_structrue(); 
}

duplication::duplication(FILE *fd, int ws, int ass, int mss, int mxx){
  window_size = ws;
  avg_seg_size = ass;
  min_seg_size = mss;
  max_seg_size = mxx;
	//fname[PATH_MAX] = {0};
	memset(fname, 0, PATH_MAX);
	logfd = fd;

	init_rabin_structrue();
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
		std::string s(path);
	
		//get fingerprint	
		vector<MD5_code> code_list;
		fingerprint(path, code_list);
		
		//if file exist
		map<string, vector<MD5_code> >::iterator iter;
		iter = file_map.find(s);
		if(iter == file_map.end()){
			//file_map.put(s, code_list);
			file_map.insert(pair<string, vector<MD5_code> >(s, code_list));	
		}

		//update chunk
		update_chunk(path, code_list);												
}

void duplication::update_chunk(const char *fpath, vector<MD5_code> &code_list){
	//vector<MD5_code>::iterator iter;
	map<MD5_code, int>::iterator iter;
	long offset = 0;
	for(unsigned int i = 0; i < code_list.size(); i++){
		MD5_code c = code_list[i];	
		if((iter = chunk_set.find(c)) != chunk_set.end()){
			iter->second += 1;	
		}else{
			//chunk_set.put(c, 1);
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
	remove(fpath);
	deduplicate(fpath);	
}

void duplication::remove(const char *fpath){
  string s(fpath);
  map<string, vector<MD5_code> >::iterator iter;
  if((iter = file_map.find(s)) == file_map.end()){
    log_msg("LancerFS error: not %s exist in index\n", fpath);
  	return;
	}

	vector<MD5_code> chunks = file_map[s];
	map<MD5_code, int>::iterator chunk_iter;	
	for(unsigned int i = 0; i < chunks.size(); i++){
		MD5_code c = chunks[i];

		cloud_delete_object("bkt", c.md5);	
		if((chunk_iter = chunk_set.find(c)) != chunk_set.end()){
			if(chunk_iter->second == 1){
				chunk_set.erase(chunk_iter);
			}else{
				chunk_iter->second -= 1;
			}		
		}	
	} 
}

void duplication::retrieve(const char *fpath){
	string s(fpath);
	map<string, vector<MD5_code> >::iterator iter;
	if((iter = file_map.find(s)) == file_map.end()){
		log_msg("LancerFS error: not %s exist in index\n", fpath);
		//exit(1);					
		return;	
	}			
	
	vector<MD5_code> chunks = file_map[s];
	long offset = 0;
	for(unsigned int i = 0; i < chunks.size(); i++){
		MD5_code c = chunks[i];
		get(fpath, c, offset);
		offset += c.segment_len;	
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
  int len, segment_len = 0, b;
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

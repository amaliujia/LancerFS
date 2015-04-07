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
		fingerprint(path, 5_code> &code_list);
		
		//if file exist
		std::unordered_map<std::string, MD5_code>::iterator iter;
		iter = file_map.find(s);
		if(iter == file_map.end()){
			file_map.put(s, code_list);	
		}

		//update chunk
		update_chunk(code_list);												
					
}

void duplication::update_chunk(vector<MD5_code> &code_list){
	vector<MD5_code>::iterator iter;
	for(int i = 0; i < code_list.size(); i++){
		MD5_code c = code_list[i];	
		if((iter = chunk_set.find(c)) != chunk_set.end()){
			iter->second += 1;	
		}else{
			chunk_set.put(c, 1);	
			//push to cloud
			put(c);		
		}								
	}				
}

void duplication::put(MD5_code &code){
	
}

duplication::~duplication(){
	rabin_free(&rp);
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
				code.set(md5, segment_len, total_len);
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
  code.set(md5, segment_len);
  code_list.push_back(code);
	
	rabin_reset(rp);
}

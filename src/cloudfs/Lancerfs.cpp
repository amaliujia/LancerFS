#include "Lancerfs.hh"

LancerFS* LancerFS::_lancerFS = NULL;

LancerFS::LancerFS(){

}

LancerFS::~LancerFS(){

}

int LancerFS::getattr(const char *path, struct stat *statbuf){
	return 0;	
}

int LancerFS::readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
							off_t offset, struct fuse_file_info *fileInfo)
{
	return 0;
}

int LancerFS::mkdir(const char *path, mode_t mode){
	std::cout << "mkdir" << std::endl;
	return 0;
}

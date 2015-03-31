#include <assert.h>
#include <iostream>
#include "extmap.h"
#include <string.h>
#include <map>
#include <iomanip>
#include <fstream>
using namespace std;

map <string, int> MD5map;
map <string, int> cache_lru_map;
map <string, int> cache_size_map;

typedef map<string, int> std_map;
extern int cur_cache_capacity;

void insert(char * key, int value){
	MD5map[key] = value;
	return;
}

void cache_map_insert(char * key, int lru, int size){
	cache_lru_map[key] = lru;
	cache_size_map[key] = size;
	return;
}

//if exists, return 1;
//else, return 0;
int exist(char * key){
	if(MD5map.find(key) != MD5map.end()){
		return 1;
	}
	else{
		return 0;
	}
}

int cache_map_exist(char * key){
	assert(key);
	if(cache_lru_map.find(key) != cache_lru_map.end()){
		return 1;
	}
	else{
		return 0;
	}
}

int get(char *key){
	return MD5map[key];
}

int cache_map_get_lru(char *key){
	return cache_lru_map[key];
}

int cache_map_get_size(char *key){
	return cache_size_map[key];
}

void erase(char * key){
	MD5map.erase(key);
	cache_lru_map.erase(key);
	cache_size_map.erase(key);
}

void cache_map_erase(char * key){
	cache_lru_map.erase(key);
	cache_size_map.erase(key);
}

void md5_map_it(char *path){
	
	ofstream md5_backup;
	md5_backup.open(path);

	for(std_map::iterator i = MD5map.begin(); i != MD5map.end(); i++){
		cout << "key: " << i->first << " value:" << i->second << " \n"; 
		md5_backup << i->first << " " << setfill('0') << setw(4) << i->second << "\n"; 
	}
	md5_backup.close();
}

int cache_map_it(char *path){
	ofstream cache_backup;
	cache_backup.open(path);
	int j = 0;
	int size = 0;
	for(std_map::iterator i = cache_lru_map.begin(); i != cache_lru_map.end(); i++){
		j++;
		size += cache_size_map[i->first];
		cout << "key: " << i->first << " lru:" << i->second << " size:" << cache_size_map[i->first] << " \n"; 
		cache_backup << i->first << " " << setfill('0') << setw(4) << cache_size_map[i->first] << "\n";
	}
	cache_backup.close();
	cur_cache_capacity = size;
	cout << "cur_cache_capacity: " << cur_cache_capacity;
	return j;
}

void update_LRU(char *key){
	for(std_map::iterator i = cache_lru_map.begin(); i != cache_lru_map.end(); i++){
		// cout << "key: " << i->first << " value:" << i->second << " \n"; 

		string key_string = key;
		// cout << "key: " << key_string << endl;
		if(key_string == i->first){
			cache_lru_map[i->first] = 0;
		}
		else{
			int lru = cache_lru_map[i->first];
			lru++;
			cache_lru_map[i->first] = lru;
		}
	}
}

char * eviction(){
	int max_lru = 0;
	string to_evict;
	for(std_map::iterator i = cache_lru_map.begin(); i != cache_lru_map.end(); i++){
		if(i->second > max_lru){
			max_lru = i->second;
			to_evict = i->first;
		}
	}
	int evict_size = cache_size_map[to_evict];
	cur_cache_capacity -= evict_size;
	cache_lru_map.erase(to_evict);
	cache_size_map.erase(to_evict);
	char * retval = new char[to_evict.length() + 1];
	strcpy(retval, to_evict.c_str());
	return retval;
}
#include <stdio.h> 
#include <stdlib.h>
#include <assert.h> 
#include <string.h>
#include <unistd.h>  
#include <math.h> 
#include "bloom.h"

///////////////////////////////////////////////////////////////////////////////
//                             BLOOM FILTER                                  //
///////////////////////////////////////////////////////////////////////////////
int bloom_bitmap[BLOOM_SZ] = {0};        // bloom filter for all nodes in memory

unsigned hash1(const char* str, int len) {
	unsigned int x = 548957;
	unsigned int y = 93683;
	unsigned int hash = 0; 

	for (int i = 0; i < len; i++) {
		hash = hash * x + (*str); 
		x *= y; 
		str++;
	}

	return hash % BLOOM_SZ;
}

unsigned hash2(const char* str, int len) {
	unsigned int seed = 97;
	unsigned int hash = 0; 

	for (int i = 0; i < len; i ++) {
		hash = (hash * seed) + (*str); 
		str++;
	}

	return hash % BLOOM_SZ;
}

unsigned hash3(const char* str, int len) {
	unsigned int hash = 5381; 
	for (int i = 0; i < len; i ++) {
		hash = ((hash << 5) + hash) + (*str); 
		str++;
	}
	return hash % BLOOM_SZ;
}

unsigned hash4(const char* str, int len) {
	unsigned int hash = len; 
	for (int i = 0; i < len; i ++) {
		hash = ((hash << 5) ^ (hash >> 27)) ^ (*str); 
		str++;
	}
	return hash % BLOOM_SZ;
}

unsigned hash5(const char *str, int len) {
	unsigned int hash = 0; 
	for (int i = 0; i < len; i ++) {
		hash = (*str) + (hash << 6) + (hash << 16) - hash; 
		str++;
	}
	return hash % BLOOM_SZ;
}

unsigned hash6(const char *str, int len) {
	unsigned hash = 0; 
	for (int i = 1; i < len; i++) {
		hash += (5 * atoi(&str[i]) + 7 * len) % BLOOM_SZ; 
	}
	return hash % BLOOM_SZ;
}

unsigned hash7(const char *str, int len) {
	unsigned sum = 0;
	for (int i = 1; i < len; i++) {
		sum += atoi(&str[i]);
	}
	return sum % BLOOM_SZ;
}

void bf_insert(char* key) {
	int len = strlen(key);
	int hash = 0; 
	hash = hash1(key, len);
	set(bloom_bitmap,hash);

	hash = hash2(key, len);
	set(bloom_bitmap,hash); 

	hash = hash3(key, len);
	set(bloom_bitmap,hash); 

	hash = hash4(key, len);
	set(bloom_bitmap,hash);

	hash = hash5(key, len);
	set(bloom_bitmap,hash);

	hash = hash6(key, len);
	set(bloom_bitmap,hash);

	hash = hash7(key, len);
	set(bloom_bitmap,hash);
}

int bf_search(char* key) {
	int len = strlen(key) - 1; 
	int hash = hash1(key, len); 
	if (test(bloom_bitmap, hash))
		return 0; 
	hash = hash2(key, len); 
	if (test(bloom_bitmap, hash))
		return 0; 
	hash = hash3(key, len); 
	if (test(bloom_bitmap, hash))
		return 0; 
	hash = hash4(key, len); 
	if (test(bloom_bitmap, hash))
		return 0; 
	hash = hash5(key, len); 
	if (test(bloom_bitmap, hash))
		return 0; 
	hash = hash6(key, len); 
	if (test(bloom_bitmap, hash))
		return 0; 
	hash = hash7(key, len); 
	if (test(bloom_bitmap, hash))
		return 0; 
	return 1;
}























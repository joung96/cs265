#include <stdio.h> 
#include <stdlib.h>
#include <assert.h> 
#include <string.h>
#include "lsm.h"
#include <unistd.h>  
#include <math.h> 
#include "bloom.h"

///////////////////////////////////////////////////////////////////////////////
//                             BLOOM FILTER                                  //
///////////////////////////////////////////////////////////////////////////////

unsigned hash1(const char* str, int len, int modulo) {
	unsigned int x = 548957;
	unsigned int y = 93683;
	unsigned int hash = 0; 

	for (int i = 0; i < len; i++) {
		hash = hash * x + (*str); 
		x *= y; 
		str++;
	}

	return hash % modulo;
}

unsigned hash2(const char* str, int len, int modulo) {
	unsigned int seed = 97;
	unsigned int hash = 0; 

	for (int i = 0; i < len; i ++) {
		hash = (hash * seed) + (*str); 
		str++;
	}

	return hash % modulo;
}

unsigned hash3(const char* str, int len, int modulo) {
	unsigned int hash = 5381; 
	for (int i = 0; i < len; i ++) {
		hash = ((hash << 5) + hash) + (*str); 
		str++;
	}
	return hash % modulo;
}

unsigned hash4(const char* str, int len, int modulo) {
	unsigned int hash = len; 
	for (int i = 0; i < len; i ++) {
		hash = ((hash << 5) ^ (hash >> 27)) ^ (*str); 
		str++;
	}
	return hash % modulo;
}

unsigned hash5(const char *str, int len, int modulo) {
	unsigned int hash = 0; 
	for (int i = 0; i < len; i ++) {
		hash = (*str) + (hash << 6) + (hash << 16) - hash; 
		str++;
	}
	return hash % modulo;
}

unsigned hash6(const char *str, int len, int modulo) {
	unsigned hash = 0; 
	for (int i = 1; i < len; i++) {
		hash += (5 * atoi(&str[i]) + 7 * len) % modulo; 
	}
	return hash % modulo;
}

unsigned hash7(const char *str, int len, int modulo) {
	unsigned sum = 0;
	for (int i = 1; i < len; i++) {
		sum += atoi(&str[i]);
	}
	return sum % modulo;
}

void bf_insert(char* key, struct block *block) {
	int len = strlen(key);
	int num_hash = 1;
	int hash = 0; 

	if (num_hash > block->num_hashes)
		return;
	hash = hash1(key, len, block->num_bits);
	set(block->bloom_filter,hash);
	num_hash++;

	if (num_hash > block->num_hashes)
		return;
	hash = hash2(key, len, block->num_bits);
	set(block->bloom_filter,hash);
	num_hash++;

	if (num_hash > block->num_hashes)
		return;
	hash = hash3(key, len, block->num_bits);
	set(block->bloom_filter,hash);
	num_hash++;

	if (num_hash > block->num_hashes)
		return;
	hash = hash4(key, len, block->num_bits);
	set(block->bloom_filter,hash);
	num_hash++;

	if (num_hash > block->num_hashes)
		return;
	hash = hash5(key, len, block->num_bits);
	set(block->bloom_filter,hash);
	num_hash++;

	if (num_hash > block->num_hashes)
		return;
	hash = hash6(key, len, block->num_bits);
	set(block->bloom_filter,hash);
	num_hash++;

	if (num_hash > block->num_hashes)
		return;
	hash = hash7(key, len, block->num_bits);
	set(block->bloom_filter,hash);
	num_hash++;
}

int bf_search(char* key, struct block *block) {
	int len = strlen(key); 
	int num_hash = 1;
	int hash;

	if (num_hash > block->num_hashes)
		return 1;
	hash = hash1(key, len, block->num_bits);
	if (!test(block->bloom_filter, hash))
		return 0; 
	num_hash++;

	if (num_hash > block->num_hashes)
		return 1;
	hash = hash2(key, len, block->num_bits);
	if (!test(block->bloom_filter, hash))
		return 0; 
	num_hash++;

	if (num_hash > block->num_hashes)
		return 1;
	hash = hash3(key, len, block->num_bits); 
	if (!test(block->bloom_filter, hash))
		return 0; 
	num_hash++;

	if (num_hash > block->num_hashes)
		return 1;
	hash = hash4(key, len, block->num_bits); 
	if (!test(block->bloom_filter, hash))
		return 0; 
	num_hash++;

	if (num_hash > block->num_hashes)
		return 1;
	hash = hash5(key, len, block->num_bits);
	if (!test(block->bloom_filter, hash))
		return 0; 
	num_hash++;

	if (num_hash > block->num_hashes)
		return 1;
	hash = hash6(key, len, block->num_bits);
	if (!test(block->bloom_filter, hash))
		return 0; 
	num_hash++;

	if (num_hash > block->num_hashes)
		return 1;
	hash = hash7(key, len, block->num_bits);
	if (!test(block->bloom_filter, hash))
		return 0; 
	num_hash++;
	return 1;
}

void bf_refresh(struct block block, node *nodes) {
	int i; 
	char strkey[MAX_NAMELEN];
	// zero out all bits
	for (i = 0; i < block.num_bits; i++) {
		zero(block.bloom_filter, i);
	}
	// set bits
	for (i = 0 ; i < block.curr_size; i ++) {
		sprintf((char*)&strkey, "%d", nodes[i].key);
		bf_insert((char*)&strkey, &block);
	}
}











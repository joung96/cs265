#include <stdio.h> 
#include <stdlib.h>
#include <string.h>

#define MAX_NAMELEN 60
#define MAX_GETS 8192000  // 8000kB

extern int successful_gets;
extern int failed_gets; 
extern int total_gets;

typedef struct node {
	int key; 
	int val;
	int ghost;
} node;

typedef struct block {     // level
	node *nodes;
	int capacity;
	int curr_size;
	int *bloom_filter; // pointer to bloom filter for this level 
	int num_bits; 
	int num_hashes; // number of hash functions to apply to this filter
	int hi; // fence pointers
	int lo; 
} block;

typedef struct lsm_tree {
	int num_written; // number of nodes written
	struct block *blocks;     //array of blocks
	struct block *disk_blocks;
	int maxlevels;
	int blocksize;
	int *disk;
} lsm_tree; 

typedef struct thread_args {
	int start_index;
	int load_sz;
	int *allkeys;
	lsm_tree *tree;
} thread_args;


extern char *disk_names[];

lsm_tree *lsm_init(int blocksize, int multiplier, int maxlevels);

int put(int key, int value, char *strkey, lsm_tree *tree);

void remove_duplicates(node* buf_to_edit, int sz1, node* ref_buf, int sz2);

int push_to_disk(lsm_tree *tree, int disk_level);

int comparison(const void *a, const void *b);

void merge_data(node *buf, node *buf1, node *buf2, int sz1, int sz2);

int get(int key, char *strkey, int should_print, lsm_tree *tree);

int parallel_get(int key, char *strkey, int num_threads, lsm_tree *tree);

void *thread_get(void *args);

void lsm_destroy(lsm_tree *tree);

int range(int key1, int key2, lsm_tree *tree);

int delete(int key, char *strkey, lsm_tree *tree);

int stat(lsm_tree *tree);

int load(char *filename, lsm_tree *tree);
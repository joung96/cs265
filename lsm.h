#include <stdio.h> 
#include <stdlib.h>

typedef struct node {
	int key; 
	int val;
} node;

typedef struct block {
	node *nodes;
	int capacity;
	int curr_size;
} block;

typedef struct lsm_tree {
	int num_written; // number of nodes written
	struct block *blocks;     //array of blocks
	int maxlevels;
	int blocksize;
} lsm_tree; 

lsm_tree *lsm_init(int blocksize, int multiplier, int maxlevels);

int put(int key, int value, lsm_tree *tree);

int push_to_disk(lsm_tree *tree);

int comparison(const void *a, const void *b);

void merge_data(node *buf, node *buf1, node *buf2, int sz1, int sz2);

int get(int key, lsm_tree *tree);

void lsm_destroy(lsm_tree *tree);

int range(int key1, int key2, lsm_tree *tree);

int delete(int key, lsm_tree *tree);

int stat(lsm_tree *tree);

int load(const char *filename, lsm_tree *tree);
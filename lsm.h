#include <stdio.h> 
#include <stdlib.h>
#include <string.h>

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
	struct block *disk_blocks;
	int maxlevels;
	int blocksize;
	int *disk;
} lsm_tree; 

char *disk_names[] = {"disk0.txt", "disk1.txt", "disk3.txt", "disk4.txt", "disk5.txt"};

lsm_tree *lsm_init(int blocksize, int multiplier, int maxlevels);

int put(int key, int value, lsm_tree *tree);

void remove_duplicates(node* buf_to_edit, int sz1, node* ref_buf, int sz2);

int push_to_disk(lsm_tree *tree, int disk_level);

int comparison(const void *a, const void *b);

void merge_data(node *buf, node *buf1, node *buf2, int sz1, int sz2);

int get(int key, lsm_tree *tree);

void lsm_destroy(lsm_tree *tree);

int range(int key1, int key2, lsm_tree *tree);

int delete(int key, lsm_tree *tree);

int stat(lsm_tree *tree);

int load(char *filename, lsm_tree *tree);

#include <stdio.h> 
#include <stdlib.h>

#define BLOCKSIZE 50
#define DATASIZE 15


typedef struct node {
	int key; 
	int val;
} node;

typedef struct lsm_tree {
	int capacity; 
	int curr_size;
	int num_written; // number of nodes written
	node *block;     //array of nodes
	int is_sorted:1;
} lsm_tree; 

lsm_tree *lsm_init(void);

int put(int key, int value, lsm_tree *tree);

int push_to_disk(lsm_tree *tree);

int comparison(const void *a, const void *b);

void merge_data(node *buf, node *buf1, node *buf2, int sz1, int sz2);

int get(int key, lsm_tree *tree);

void lsm_destroy(lsm_tree *tree);

void range(int key1, int key2, lsm_tree *tree);

int delete(int key, lsm_tree *tree);
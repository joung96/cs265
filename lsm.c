#include <stdio.h> 
#include <stdlib.h>
#include <assert.h> 
#include <string.h>
#include <unistd.h>  
#include <math.h> 
#include "lsm.h"

///////////////////////////////////////////////////////////////////////////////
//                                  LSM                                      //
///////////////////////////////////////////////////////////////////////////////

lsm_tree *lsm_init(int blocksize, int multiplier, int maxlevels) {
	lsm_tree *tree;
	block *block;

	// allocate memory for the tree struct skeleton
	tree = malloc(sizeof(lsm_tree));
	if (!tree) {
		perror("Initial tree malloc failed");
		return  NULL;
	}
	// fill the tree with the right number of blocks
	tree->blocks = malloc(sizeof(block) * sizeof(node) * maxlevels);

	// fill each block with the correct number of nodes
	for (int i = 0; i < maxlevels; i++) {

		// calculate node capcity based on level and multiplier
		int capacity = blocksize * pow(multiplier, i);
		tree->blocks[i].nodes = malloc(sizeof(node) * capacity);
		if (!tree->blocks[i].nodes) {
			perror("Tree node list malloc failed");
			return NULL;
		}
		tree->blocks[i].capacity = capacity;
		tree->blocks[i].curr_size = 0;
	}
	tree->num_written = 0;
	tree->maxlevels = maxlevels; 
	tree->blocksize = blocksize;

	return tree;
}

void lsm_destroy(lsm_tree *tree) { 
	// free nodes, then blocks, then tree 
	for (int i = 0; i < tree->maxlevels; i++) 
		free(tree->blocks[i].nodes);
	free(tree->blocks); 
	free(tree);
}

///////////////////////////////////////////////////////////////////////////////
//                                  PUT                                      //
///////////////////////////////////////////////////////////////////////////////

int put(int key, int value, lsm_tree *tree) {
	int result;
	int i; 
	int sum; 

	//fill a new node with the key/value
	node newnode; 
	newnode.key = key; 
	newnode.val = value;
 
	// first level is full so some merging must happen
	if (tree->blocks[0].curr_size == tree->blocks[0].capacity) {

		// at the very least we need to push to level 1
		i = 1; 

		// iterate to calculate what level we need to push to 
		while(i < tree->maxlevels) {
			sum = tree->blocks[i].curr_size + tree->blocks[i - 1].curr_size;

			// if the sum does not exceed capacity, the correct level is stored in i
			if (sum <= tree->blocks[i].capacity)
				break;
			i++;
		}

		mergesort(tree->blocks[i - 1].nodes, tree->blocks[i - 1].curr_size, sizeof(node), comparison);

		// if we must push to disk, push first before touching RAM buffers
		if (i == tree->maxlevels) {
			result = push_to_disk(tree);
			if (result != 0) {
				perror("Push to disk failed");
				return -1;
			}
			i--;
		}

		// push changes throughout the tree
		while(i != 0) {
			mergesort(tree->blocks[i].nodes, tree->blocks[i].curr_size, sizeof(node), comparison);
			remove_duplicates(tree->blocks[i].nodes, tree->blocks[i].curr_size, tree->blocks[i - 1].nodes, tree->blocks[i - 1].curr_size);

			node *newnodes = malloc(sizeof(node) * tree->blocks[i].capacity);

			// merge level i and level (i - 1)
			merge_data(newnodes, tree->blocks[i].nodes, tree->blocks[i - 1].nodes, 
				tree->blocks[i].curr_size, tree->blocks[i - 1].curr_size); 
			memcpy(tree->blocks[i].nodes, newnodes, sizeof(node) * tree->blocks[i].capacity);
			free(newnodes);

			// clear level (i - 1)
			tree->blocks[i].curr_size += tree->blocks[i - 1].curr_size;
			tree->blocks[i - 1].curr_size = 0;
			i--;
		}

	}

	// at this point we know there is enough space in level 0
	assert(tree->blocks[0].curr_size < tree->blocks[0].capacity);

	// insert newnode
	tree->blocks[0].nodes[tree->blocks[0].curr_size] = newnode;
	tree->blocks[0].curr_size++;

	return 0;

}

// takes two nodes and compares key values
int comparison(const void *a, const void *b) {
	node *aa = (node *) a; 
	node *bb = (node *) b; 

	if (aa->key < bb->key) 
		return -1; 
	else if (aa->key > bb->key) 
		return 1; 
	return 0; 
}

// int read_block(int block_number, node *buffer, lsm_tree *tree) { 
// 	FILE *file = fopen("disk.txt", "r");
// 	fseek(file, sizeof(node) * (tree->blocksize * block_number),SEEK_SET);
// 	fread(buffer, sizeof(node), tree->blocksize, file);
// 	fclose(file);
// 	return 0;
// }

// takes two buffers and removes all nodes that are present in both from buf_to_edit
void remove_duplicates(node* buf_to_edit, int sz1, node* ref_buf, int sz2) {
	for (int j = 0; j < sz1; j++) {
		for (int k = 0; k < sz2; k++) {
			if (buf_to_edit[j].key == ref_buf[k].key) {
				memmove(&buf_to_edit[j], &buf_to_edit[j + 1], sizeof(node) * (sz1- j));
				sz1--;
			}
		}
	}
}

int push_to_disk(lsm_tree *tree) { 
	node *all_data_buffer = NULL;
	node *disk_buffer = NULL;
	FILE *file = NULL;
	int result = 0;
	int need_to_free = 0;

	// if the file exists
	if (access("disk.txt", F_OK) != -1) {
		file = fopen("disk.txt", "r");

		disk_buffer = malloc(tree->num_written * sizeof(node)); 
		if (!disk_buffer) {
			fclose(file); 
			perror("Disk buffer malloc failed");
			return -1;
		}

		// read all nodes from disk buffer
		fread(disk_buffer, sizeof(node), tree->num_written, file); 

		all_data_buffer = malloc(sizeof(node) * (tree->num_written + tree->blocks[tree->maxlevels - 1].curr_size));
		if (!all_data_buffer) {
			fclose(file);
			free(disk_buffer); 
			perror("All data buffer malloc failed");
			return -1;
		}
		need_to_free = 1;

		remove_duplicates(disk_buffer, tree->num_written, tree->blocks[tree->maxlevels - 1].nodes, tree->blocks[tree->maxlevels - 1].curr_size);

		// merge disk nodes with the lowest level of the lsm tree
		merge_data(all_data_buffer, disk_buffer, tree->blocks[tree->maxlevels - 1].nodes, 
			       tree->num_written, tree->blocks[tree->maxlevels - 1].curr_size);

		result = fclose(file); 
		if (result) {
			free(disk_buffer);
			free(all_data_buffer);
			perror("file close failed 1"); 
			return -1;
		}
	}

	file = fopen("disk.txt", "w");  
	if (! all_data_buffer) {
		// if this is the first time writing to disk just write the last level of the tree
		all_data_buffer = tree->blocks[tree->maxlevels - 1].nodes;
	}

	// write the node buffer to disk 
	result = fwrite(all_data_buffer, sizeof(node), 
		(tree->num_written + tree->blocks[tree->maxlevels - 1].curr_size), file);

	// cleanup malloced buffers
	if(need_to_free)
		free(all_data_buffer); 
	if (disk_buffer) 
		free(disk_buffer);

	if (!result) {
		fclose(file); 
		perror("fwrite failed while writing to disk 1");
		return -1;
	}

	result = fclose(file); 
	if (result) {
		perror("file close failed 2"); 
		return -1;
	}
	
	// bookkeeping
	tree->num_written += tree->blocks[tree->maxlevels - 1].curr_size;
	tree->blocks[tree->maxlevels - 1].curr_size = 0;

	return 0; 
}

// merge two sorted buffers
// merge sort inspired by (http://www.thecrazyprogrammer.com/2014/03/c-program-for-implementation-of-merge-sort.html)
void merge_data(node *buf, node *left, node *right, int sz1, int sz2) {
	int i = 0;
	int j = 0; 
	int k = 0;
	while (i < sz1 && j < sz2) {
		if (left[i].key < right[j].key) 
			buf[k++] = left[i++];
		else 
			buf[k++] = right[j++]; 
	}
	while (i < sz1) 
		buf[k++] = left[i++];
	while (j < sz2) 
		buf[k++] = right[j++];
}

///////////////////////////////////////////////////////////////////////////////
//                                  GET                                      //
///////////////////////////////////////////////////////////////////////////////
int get(int key, lsm_tree *tree) {
	int result; 

	// need a temporary storage location for the key to get
	node keynode;
	node *found; 
	keynode.key = key;

	// sort the toplevel -- lowerlevels are sorted because of the push implementation
	mergesort(tree->blocks[0].nodes, tree->blocks[0].curr_size, sizeof(node), comparison);
	for (int i = 0; i < tree->maxlevels;i++) {
		// search for the node
		found = bsearch(&keynode, tree->blocks[i].nodes, tree->blocks[i].curr_size, sizeof(node), comparison);
	    if (found) {
    		printf("%d\n", found->val);
    		return 0;
    	}

	}
	// did not find in RAM -- try search on disk
	if (access("disk.txt", F_OK) != -1) {
		FILE *file = fopen("disk.txt", "r"); 
		if (!file) {
			perror("file open in get failed");
			return -1;
		}

		node *disk_buffer = malloc(tree->num_written * sizeof(node)); 
		if (!disk_buffer) {
			perror("Disk buffer malloc failed");
			fclose(file);
			return -1;
		}

		// read all nodes in from disk 
		result = fread(disk_buffer, sizeof(node), tree->num_written, file); 

		result = fclose(file);
		if (result) {
			free(disk_buffer);
			perror("File close failed in get");
			return -1;
		}	

		// search disk nodes
		found = bsearch(&keynode, disk_buffer, tree->num_written, sizeof(node), comparison);
	    if (found) {
	    	printf("%d\n", found->val);
	    	free(disk_buffer);
	    	return 0;
	    }
	    free(disk_buffer);
	} 
	// not found
	printf("\n");
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
//                                RANGE                                      //
///////////////////////////////////////////////////////////////////////////////

int range(int key1, int key2, lsm_tree *tree){
	int result; 

	assert(key1 <= key2);
	qsort(tree->blocks[0].nodes, tree->blocks[0].curr_size, sizeof(node), comparison);

	// array that indicates whether or not we have not found the element
	int* foundlist = NULL;
	int num_found = 0;

	for (int i = 0; i < tree->maxlevels;i++) {
		for (int j = 0; j < tree->blocks[i].curr_size; j++) {
			if (tree->blocks[i].nodes[j].key >= key2)
				break;
			if (tree->blocks[i].nodes[j].key >= key1) {

				// it's a match -- but if we've seen this key before don't print again
				int print = 1;
				for (int k = 0; k < num_found; k++) {
					if (tree->blocks[i].nodes[j].key == foundlist[k]) {
						print = 0; 
						break;
					}
				}
				if (print) {
					printf("%d:%d\n", tree->blocks[i].nodes[j].key, tree->blocks[i].nodes[j].val);
					num_found++;
					foundlist = realloc(foundlist, sizeof(in t) * num_found);
					foundlist[num_found - 1] = tree->blocks[i].nodes[j].key;
				}
			}
		}
	}

	// if we've found all the numbers, we are done. 
	if ((key2 - key1 + 1) == num_found) 
		return 0;

	// perhaps the rest are on disk!
	if (access("disk.txt", F_OK) != -1) {
		FILE *file = fopen("disk.txt", "r"); 
		if (!file) {
			perror("file open in get failed");
			free(foundlist);
			return -1;
		}

		// read in all nodes from disk
		node *disk_buffer = malloc(tree->num_written * sizeof(node)); 
		if (!disk_buffer) {
			perror("Disk buffer malloc failed");
			free(foundlist);
			fclose(file);
			return -1;
		}

		result = fread(disk_buffer, sizeof(node), tree->num_written, file); 

		result = fclose(file);
		if (result) {
			free(disk_buffer);
			free(foundlist);
			perror("File close failed in get");
			return -1;
		}	

		for (int i = 0; i < tree->num_written; i++) {
			if (disk_buffer[i].key >= key2)
				break;
			if (disk_buffer[i].key >= key1) {
				// it's a match -- but if we've seen this key before don't print again
				int print = 1;
				for (int k = 0; k < num_found; k++) {
					if (disk_buffer[i].key == foundlist[k]) {
						print = 0; 
						break;
					}
				}
				if (print) {
					printf("%d:%d\n", disk_buffer[i].key, disk_buffer[i].val);
					num_found++;
					foundlist = realloc(foundlist, sizeof(int) * num_found);
					foundlist[num_found - 1] = disk_buffer[i].key;
				}
			}
		}

	    free(disk_buffer);
	} 
	free(foundlist);
	return 0;
}


///////////////////////////////////////////////////////////////////////////////
//                                DELETE                                     //
///////////////////////////////////////////////////////////////////////////////

int delete(int key, lsm_tree *tree) {
	// check if in memory
	for (int i = 0; i < tree->maxlevels; i++) {
		for (int j = 0; j < tree->blocks[i].curr_size; j++) {
			// it's in memory! delete and return
			if (key == tree->blocks[i].nodes[j].key) {
				memmove(&tree->blocks[i].nodes[j], &tree->blocks[i].nodes[j + 1], sizeof(node) * (tree->blocks[i].curr_size - j));
				tree->blocks[i].curr_size--; 
				return 0;
			}
		}
	}

	// check if on disk
	if (access("disk.txt", F_OK) != -1) {
		FILE *file = fopen("disk.txt", "r"); 
		int result; 

		node *disk_buffer = malloc(tree->num_written * sizeof(node)); 
		if (!disk_buffer) {
			fclose(file); 
			perror("Disk buffer malloc failed");
			return -1;
		}

		// read in nodes from memory
		result = fread(disk_buffer, sizeof(node), tree->num_written, file); 
		if (result == 0) { 
			fclose(file); 
			free(disk_buffer);
			perror("fread failed while deleting");
			return -1;
		}	

		result = fclose(file); 
		if (result) {
			perror("file close failed 2"); 
			free(disk_buffer);
			return -1;
		}

		for (int i = 0; i < tree->num_written; i ++) {
			// found the key on disk! delete and return
			if (key == disk_buffer[i].key) {
				disk_buffer[i] = disk_buffer[tree->num_written - 1];
				tree->num_written--;
				mergesort(disk_buffer, tree->num_written, sizeof(node), comparison);

				file = fopen("disk.txt", "w");  
				result = fwrite(disk_buffer, sizeof(node), tree->num_written, file);
				if (!result) {
					free(disk_buffer);
					perror("fwrite failed while writing to disk 1");
					return -1;	
				}

				result = fclose(file); 
				if (result) {
					free(disk_buffer);
					perror("file close failed 2"); 
					return -1;
				}
				free(disk_buffer);
				return 0;
			}
		}
	} 
	// unable to find key to delete
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
//                                LOAD                                       //
///////////////////////////////////////////////////////////////////////////////
int load(char *filename, lsm_tree *tree) {
	int filelen; 
	int result; 

	FILE *file = fopen(filename, "rb");
	if (!file) {
		perror("merp");
		return -1;
	}

	// calculate how much storage is needed to hold the .dat file
	fseek(file, 0, SEEK_END); 
	filelen = ftell(file);  
	rewind(file);   

	int *buffer = malloc((filelen + 1) * sizeof(int)); 
	if (!buffer) {
		perror("failed to malloc in load");
		return -1;
	}
	int num_ints = 0; 

	// read into the buffer we've created one integer at a time
	for (int i = 0; i < filelen; i++){
		result = fread(buffer + i, 4, 1, file);
		if(result < 0) {
			perror("read failed while loading");
			return -1;
		}

		num_ints++;
	}
	result = fclose(file);
	if (result) {
		perror("failed to close file in load");
		return -1;
	}

	// read pairs of numbers from the buffer into a put query
	for (int i = 0; i < num_ints; i += 2) 
		put(buffer[i], buffer[i+1], tree);
	return 0;
}


///////////////////////////////////////////////////////////////////////////////
//                                PRINT STAT                                 //
/////////////////////////////////////////////////////////////////////////////
int stat(lsm_tree *tree) {
	int total_pairs = 0; 

	// gather statistics 
	for (int i = 0; i < tree->maxlevels; i ++) {
		total_pairs += tree->blocks[i].curr_size;
	}

	// print statistics
	printf("Total Pairs: %d\n", total_pairs);

	for (int i = 0; i < tree->maxlevels; i++) {
		if (tree->blocks[i].curr_size != 0) {
			printf("LVL%d: %d ", i + 1, tree->blocks[i].curr_size);
		}
	}
	printf("\n");

	for (int i = 0; i < tree->maxlevels; i++) {
		for(int j = 0; j < tree->blocks[i].curr_size; j++) {
			printf("%d:%d:%d ", tree->blocks[i].nodes[j].key,
				tree->blocks[i].nodes[j].val, i + 1);
		}
		printf("\n");
	}

	return 0;
}


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

	tree = malloc(sizeof(lsm_tree));
	if (!tree) {
		perror("Initial tree malloc failed");
		return  NULL;
	}

	tree->blocks = malloc(sizeof(block) * sizeof(node) * maxlevels);

	for (int i = 0; i < maxlevels; i++) {
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
	for (int i = 0; i < tree->maxlevels; i++) {
		free(tree->blocks[i].nodes);
	}

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
 
	// first level is full!
	if (tree->blocks[0].curr_size == tree->blocks[0].capacity) {
		i = 1; 
		while(i < tree->maxlevels) {
			sum = tree->blocks[i].curr_size + tree->blocks[i - 1].curr_size;
			if (sum <= tree->blocks[i].capacity)
				break;
			i++;
		}

		// push changes throughout table
		// sort node lists
		mergesort(tree->blocks[i - 1].nodes, tree->blocks[i - 1].curr_size, sizeof(node), comparison);
		if (i == tree->maxlevels) {
			result = push_to_disk(tree);
			if (result != 0) {
				perror("Push to disk failed");
				return -1;
			}
			i--;

		}

		while(i != 0) {
			mergesort(tree->blocks[i].nodes, tree->blocks[i].curr_size, sizeof(node), comparison);
			// delete duplicates
			for (int j = 0; j < tree->blocks[i].curr_size; j++) {
				for (int k = 0; k < tree->blocks[i - 1].curr_size; k++) {
					if (tree->blocks[i].nodes[j].key == tree->blocks[i - 1].nodes[k].key) {
						// delete the entry in level i 
						memmove(&tree->blocks[i].nodes[j], &tree->blocks[i].nodes[j + 1], sizeof(node) * (tree->blocks[i].curr_size - j));

						tree->blocks[i].curr_size--;
					}
				}
			}

			node *newnodes = malloc(sizeof(node) * tree->blocks[i].capacity);

			merge_data(newnodes, tree->blocks[i].nodes, tree->blocks[i - 1].nodes, 
				tree->blocks[i].curr_size, tree->blocks[i - 1].curr_size); 
	
			memcpy(tree->blocks[i].nodes, newnodes, sizeof(node) * tree->blocks[i].capacity);
			free(newnodes);
			// clear block
			tree->blocks[i].curr_size += tree->blocks[i - 1].curr_size;
			tree->blocks[i - 1].curr_size = 0;
			i--;
		}

	}

	// insert into level 0
	// TODO delete duplicates here
	tree->blocks[0].nodes[tree->blocks[0].curr_size] = newnode;
	tree->blocks[0].curr_size++;

	return 0;

}

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

int push_to_disk(lsm_tree *tree) { 
	node *all_data_buffer = NULL;
	node *disk_buffer = NULL;
	FILE *file = NULL;
	int result = 0;
	int need_to_free = 0;


	// file exists
	if (access("disk.txt", F_OK) != -1) {
		file = fopen("disk.txt", "r");

		disk_buffer = malloc(tree->num_written * sizeof(node)); 
		if (!disk_buffer) {
			fclose(file); 
			perror("Disk buffer malloc failed");
			return -1;
		}

		fread(disk_buffer, sizeof(node), tree->num_written, file); 

		all_data_buffer = malloc(sizeof(node) * (tree->num_written + tree->blocks[tree->maxlevels - 1].curr_size));
		if (!all_data_buffer) {
			fclose(file);
			free(disk_buffer); 
			perror("All data buffer malloc failed");
			return -1;
		}
		need_to_free = 1;

		// remove duplicates
		for (int j = 0; j < tree->num_written; j++) {
			for (int k = 0; k < tree->blocks[tree->maxlevels - 1].curr_size; k++) {
				if (disk_buffer[j].key == tree->blocks[tree->maxlevels - 1].nodes[k].key) {
					// delete the entry on disk
					memmove(&disk_buffer[j], &disk_buffer[j + 1], sizeof(node) * (tree->num_written - j));
					tree->num_written--;
				}
			}
		}

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
	// file DNE 
	file = fopen("disk.txt", "w");  
	if (! all_data_buffer) {
		all_data_buffer = tree->blocks[tree->maxlevels - 1].nodes;
	}
	result = fwrite(all_data_buffer, sizeof(node), 
		(tree->num_written + tree->blocks[tree->maxlevels - 1].curr_size), file);

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
	
	tree->num_written += tree->blocks[tree->maxlevels - 1].curr_size;
	tree->blocks[tree->maxlevels - 1].curr_size = 0;

	return 0; 
}

// merge two buffers
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
	node keynode;
	node *found; 
	keynode.key = key;

	// TODO figure out sorts
	mergesort(tree->blocks[0].nodes, tree->blocks[0].curr_size, sizeof(node), comparison);
	for (int i = 0; i < tree->maxlevels;i++) {
		found = bsearch(&keynode, tree->blocks[i].nodes, tree->blocks[i].curr_size, sizeof(node), comparison);
	    if (found) {
    		//printf("%d\n", found->val);
    		return 0;
    	}

	}
	// perhaps on disk!
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

		result = fread(disk_buffer, sizeof(node), tree->num_written, file); 

		result = fclose(file);
		if (result) {
			free(disk_buffer);
			perror("File close failed in get");
			return -1;
		}	

		found = bsearch(&keynode, disk_buffer, tree->num_written, sizeof(node), comparison);
	    if (found) {
	    	//printf("%d\n", found->val);
	    	free(disk_buffer);
	    	return 0;
	    }
	    free(disk_buffer);
	} 
	// not found
	//printf("\n");
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
//                                RANGE                                      //
///////////////////////////////////////////////////////////////////////////////

// int range(int key1, int key2, lsm_tree *tree){
// 	int result; 

// 	assert(key1 <= key2);
// 	qsort(tree->blocks[0].nodes, tree->blocks[0].curr_size, sizeof(node), comparison);

// 	// array that indicates whether or not we have not found the element
// 	char *bitmap = malloc(sizeof(char) * (key2 - key1));
// 	for (int i = 0; i < (key2 - key1); i++) 
// 		bitmap[i] = 'o'; 

// 	for (int i = 0; i < tree->maxlevels;i++) {
// 		for (int j = 0; j < tree->blocks[i].curr_size; j++) {
// 			if (tree->blocks[i].nodes[j].key >= key2)
// 				break;
// 			if (tree->blocks[i].nodes[j].key >= key1) {
// 				if (bitmap[key2 - tree->blocks[i].nodes[j].key] == 'o') {
// 					bitmap[key2 - tree->blocks[i].nodes[j].key] = 'x';
// 		    		printf("%d:%d\n", tree->blocks[i].nodes[j].key, tree->blocks[i].nodes[j].val);
// 				}
// 			}
// 		}
// 	}

// 	// are we done..?
// 	int sum = 0;
// 	for (int i = key1; i < key2; i++) {
// 		sum += bitmap[i - key1];
// 	}
// 	// yes we are done
// 	if (sum == 0) {
// 		free(bitmap);
// 		return 0;
// 	}

// 	// perhaps the rest are on disk!
// 	if (access("disk.txt", F_OK) != -1) {
// 		FILE *file = fopen("disk.txt", "r"); 
// 		if (!file) {
// 			perror("file open in get failed");
// 			free(bitmap);
// 			return -1;
// 		}

// 		node *disk_buffer = malloc(tree->num_written * sizeof(node)); 
// 		if (!disk_buffer) {
// 			perror("Disk buffer malloc failed");
// 			free(bitmap);
// 			fclose(file);
// 			return -1;
// 		}

// 		result = fread(disk_buffer, sizeof(node), tree->num_written, file); 

// 		result = fclose(file);
// 		if (result) {
// 			free(disk_buffer);
// 			free(bitmap);
// 			perror("File close failed in get");
// 			return -1;
// 		}	

// 		for (int i = 0; i < tree->num_written;i++) {
// 			if (disk_buffer[i].key >= key2)
// 				break;
// 			if (disk_buffer[i].key >= key1) {
// 				if (bitmap[key2 - disk_buffer[i].key] == 'o') {
// 					bitmap[key2 - disk_buffer[i].key] = 'x';
// 		    		printf("%d:%d\n", disk_buffer[i].key, disk_buffer[i].val);
// 				}
// 			}
// 		}


// 	    free(disk_buffer);
// 	} 
// 	// not found
// 	free(bitmap);
// 	return 0;
// }


///////////////////////////////////////////////////////////////////////////////
//                                DELETE                                     //
///////////////////////////////////////////////////////////////////////////////

int delete(int key, lsm_tree *tree) {
	// in memory

	for (int i = 0; i < tree->maxlevels; i++) {
		for (int j = 0; j < tree->blocks[i].curr_size; j++) {
			if (key == tree->blocks[i].nodes[j].key) {

				memmove(&tree->blocks[i].nodes[j], &tree->blocks[i].nodes[j + 1], sizeof(node) * (tree->blocks[i].curr_size - j));
				tree->blocks[i].curr_size--; 
				return 0;
			}
		}
	}
	// on disk
	if (access("disk.txt", F_OK) != -1) {
		FILE *file = fopen("disk.txt", "r"); 
		int result; 

		node *disk_buffer = malloc(tree->num_written * sizeof(node)); 
		if (!disk_buffer) {
			fclose(file); 
			perror("Disk buffer malloc failed");
			return -1;
		}

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
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
//                                LOAD                                       //
///////////////////////////////////////////////////////////////////////////////
int load(const char *filename, lsm_tree *tree) {
	int filelen; 
	int result; 

	FILE *file = fopen(filename, "rb");
	if (!file) {
		perror("merp");
		return -1;
	}
	fseek(file, 0, SEEK_END); 
	filelen = ftell(file);  
	rewind(file);   

	int *buffer = malloc((filelen + 1) * sizeof(int)); 
	if (!buffer) {
		perror("failed to malloc in load");
		return -1;
	}
	int num_ints = 0; 

	for (int i = 0; i < filelen; i++){
		result = fread(buffer + i, 1, 1, file);
		if(result == 0) {
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

	for (int i = 0; i < num_ints; i += 2) {	
		put(buffer[i], buffer[i+1], tree);
	}
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













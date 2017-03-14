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

lsm_tree *lsm_init(void) {
	lsm_tree *tree;
	block *block;

	tree = malloc(sizeof(lsm_tree));
	if (!tree) {
		perror("Initial tree malloc failed");
		return  NULL;
	}

	tree->blocks = malloc(sizeof(block) * sizeof(node) * MAX_LEVELS);

	for (int i = 0; i < MAX_LEVELS; i++) {
		int capacity = BLOCKSIZE * pow(MULTIPLIER, i);
		tree->blocks[i].nodes = malloc(sizeof(node) * capacity);
		if (!tree->blocks[i].nodes) {
			perror("Tree node list malloc failed");
			return NULL;
		}
		tree->blocks[i].capacity = capacity;
		tree->blocks[i].curr_size = 0;
	}
	tree->num_written = 0;

	return tree;
}

void lsm_destroy(lsm_tree *tree) { 
	for (int i = 0; i < tree->num_curr_blocks; i++) {
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
		while(i < MAX_LEVELS) {
			sum = tree->blocks[i].curr_size + tree->blocks[i - 1].curr_size;
			if (sum <= tree->blocks[i].capacity)
				break;
			i++;
		}

		// push changes throughout table
		// sort node lists
		qsort(tree->blocks[i - 1].nodes, tree->blocks[i - 1].curr_size, sizeof(node), comparison);
		if (i == MAX_LEVELS) {
			result = push_to_disk(tree);
			if (result != 0) {
				perror("Push to disk failed");
				return -1;
			}
			i--;

		}

		while(i != 0) {
			qsort(tree->blocks[i].nodes, tree->blocks[i].curr_size, sizeof(node), comparison);
			// delete duplicates
			for (int j = 0; j < tree->blocks[i].curr_size; j++) {
				for (int k = 0; k < tree->blocks[i - 1].curr_size; k++) {
					if (tree->blocks[i].nodes[j].key == tree->blocks[i - 1].nodes[k].key) {
						// delete the entry in level i 
						tree->blocks[i].nodes[j] = tree->blocks[i].nodes[tree->blocks[i].curr_size];
						qsort(tree->blocks[i].nodes, tree->blocks[i].curr_size, sizeof(node), comparison);
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

		all_data_buffer = malloc(sizeof(node) * (tree->num_written + tree->blocks[MAX_LEVELS - 1].curr_size));
		if (!all_data_buffer) {
			fclose(file);
			free(disk_buffer); 
			perror("All data buffer malloc failed");
			return -1;
		}
		need_to_free = 1;

		// remove duplicates
		for (int j = 0; j < tree->num_written; j++) {
			for (int k = 0; k < tree->blocks[MAX_LEVELS - 1].curr_size; k++) {
				if (disk_buffer[j].key == tree->blocks[MAX_LEVELS - 1].nodes[k].key) {
					// delete the entry on disk
					disk_buffer[j] = disk_buffer[tree->num_written];
					tree->num_written--;
					qsort(disk_buffer, tree->num_written, sizeof(node), comparison);
				}
			}
		}

		merge_data(all_data_buffer, disk_buffer, tree->blocks[MAX_LEVELS - 1].nodes, 
			       tree->num_written, tree->blocks[MAX_LEVELS - 1].curr_size);

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
		all_data_buffer = tree->blocks[MAX_LEVELS - 1].nodes;
	}
	result = fwrite(all_data_buffer, sizeof(node), 
		(tree->num_written + tree->blocks[MAX_LEVELS - 1].curr_size), file);

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
		fclose(file); 
		perror("file close failed 2"); 
		return -1;
	}
	
	tree->num_written += tree->blocks[MAX_LEVELS - 1].curr_size;
	tree->blocks[MAX_LEVELS - 1].curr_size = 0;

	return 0; 
}

// merge two buffers
// merge sort inspired by (http://www.thecrazyprogrammer.com/2014/03/c-program-for-implementation-of-merge-sort.html)
void merge_data(node *buf, node *left, node *right, int sz1, int sz2) {
	int i = 0;
	int j = 0; 
	int k = 0;
	while (i < sz1 && j < sz2) {
		if (left[i].key < right[j].key) {
			buf[k++] = left[i++];
		}
		else {
			buf[k++] = right[j++]; 
		}
	}
	while (i < sz1) {
		buf[k++] = left[i++];
	}
	while (j < sz2) {
		buf[k++] = right[j++];
	}
}

///////////////////////////////////////////////////////////////////////////////
//                                  GET                                      //
///////////////////////////////////////////////////////////////////////////////
int get(int key, lsm_tree *tree) {
	int result; 
	node keynode;
	node *found; 
	keynode.key = key;

	qsort(tree->blocks[0].nodes, tree->blocks[0].curr_size, sizeof(node), comparison);
	for (int i = 0; i < MAX_LEVELS;i++) {
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

int range(int key1, int key2, lsm_tree *tree){
	int result; 

	assert(key1 <= key2);

	// array that indicates whether or not we have not found the element
	int bitmap[key2 - key1];
	memset(bitmap, 1, sizeof(bitmap));

	qsort(tree->blocks[0].nodes, tree->blocks[0].curr_size, sizeof(node), comparison);
	for (int i = 0; i < MAX_LEVELS;i++) {
		for (int j = key1; j < key2; j++) {
			node keynode;
			node *found; 
			keynode.key = j;

			found = bsearch(&keynode, tree->blocks[i].nodes, tree->blocks[i].curr_size, sizeof(node), comparison);
		    if (found && bitmap[j - key1]) {
		    	bitmap[j - key1] = 0;
	    		printf("%d:%d\n", j, found->val);
	    	}
	    }
	}

	// are we done..?
	int sum = 0;
	for (int i = key1; i < key2; i++) {
		sum += bitmap[i - key1];
	}
	// yes we are done
	if (sum == 0)
		return 0;

	// perhaps the rest are on disk!
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

		for (int j = key1; j < key2; j++) {
			node keynode;
			node *found; 
			keynode.key = j;

			found = bsearch(&keynode, disk_buffer, tree->num_written, sizeof(node), comparison);
		    if (found && bitmap[j - key1]) {
		    	bitmap[j - key1] = 0;
	    		printf("%d:%d\n", j, found->val);
	    	}
	    }

	    free(disk_buffer);
	} 
	// not found
	return 0;
}


///////////////////////////////////////////////////////////////////////////////
//                                DELETE                                     //
///////////////////////////////////////////////////////////////////////////////

int delete(int key, lsm_tree *tree) {
	// in memory

	for (int i = 0; i < MAX_LEVELS; i++) {
		for (int j = 0; j < tree->blocks[i].curr_size; j++) {
			if (key == tree->blocks[i].nodes[j].key) {
				tree->blocks[i].nodes[j] = tree->blocks[i].nodes[tree->blocks[i].curr_size - 1];
				tree->blocks[i].curr_size--; 
				qsort(tree->blocks[i].nodes, tree->blocks[i].curr_size, sizeof(node), comparison);
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
				qsort(disk_buffer, tree->num_written, sizeof(node), comparison);

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
	// what if file is NULL; 
	if (!file) {
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
	for (int i = 0; i < MAX_LEVELS; i ++) {
		total_pairs += tree->blocks[i].curr_size;
	}

	// print statistics
	printf("Total Pairs: %d\n", total_pairs);

	for (int i = 0; i < MAX_LEVELS; i++) {
		if (tree->blocks[i].curr_size != 0) {
			printf("LVL%d: %d", i + 1, tree->blocks[i].curr_size);
		}
	}
	printf("\n");

	for (int i = 0; i < MAX_LEVELS; i++) {
		for(int j = 0; j < tree->blocks[i].curr_size; j++) {
			printf("%d:%d:%d ", tree->blocks[i].nodes[j].key,
				tree->blocks[i].nodes[j].val, i + 1);
		}
		printf("\n");
	}

	return 0;
}













#include <stdio.h> 
#include <stdlib.h>
#include <assert.h> 
#include <string.h>
#include <unistd.h>  
#include <math.h> 
#include <pthread.h>
#include "lsm.h"
#include "bloom.h"


char *disk_names[] = {"disk0.txt", "disk1.txt", "disk3.txt", "disk4.txt", "disk5.txt"
                      ,"disk6.txt", "disk7.txt", "disk8.txt", "disk9.txt", "disk10.txt"};

///////////////////////////////////////////////////////////////////////////////
//                                  LSM                                      //
///////////////////////////////////////////////////////////////////////////////

lsm_tree *lsm_init(int blocksize, int multiplier, int maxlevels) {
	lsm_tree *tree;
	block *block;
	int num_bits;
	int capacity;
	int result;
	FILE *file = NULL;

	if (maxlevels > 10) {
		perror("maximum level is 10");
		return NULL;
	}
	// allocate memory for the tree struct skeleton
	tree = malloc(sizeof(lsm_tree));
	if (!tree) {
		perror("Initial tree malloc failed");
		return  NULL;
	}
	// fill the tree with the right number of blocks
	tree->blocks = malloc(sizeof(block) * sizeof(node) * maxlevels);
	tree->disk_blocks = malloc(sizeof(block) * sizeof(node) * maxlevels);

	// fill each block with the correct number of nodes
	for (int i = 0; i < maxlevels; i++) {
		// calculate node capcity based on level and multiplier
		capacity = blocksize * pow(multiplier, i);
		tree->blocks[i].nodes = malloc(sizeof(node) * capacity);
		if (!tree->blocks[i].nodes) {
			perror("Tree node list malloc failed");
			return NULL;
		}
		tree->blocks[i].capacity = capacity;
		tree->blocks[i].curr_size = 0;
		tree->blocks[i].hi = 0; 
		tree->blocks[i].lo = 0; 

		// formulas for optimal bloom filter params from http://hur.st/bloomfilter?n=100&p=.05
		// num_bits = ceil((capacity * log(.05)) / log(1.0 / (pow(2.0, log(2.0)))));
		// tree->blocks[i].num_bits = num_bits;
		// tree->blocks[i].bloom_filter = malloc(ceil(num_bits / 8));
		// tree->blocks[i].num_hashes = round(log(2.0) *  num_bits/ capacity);
	}

	for (int i = 0; i < maxlevels; i++) {
		capacity = tree->blocks[maxlevels - 1].capacity * pow(multiplier, i + 1);
		tree->disk_blocks[i].capacity = capacity;
		tree->disk_blocks[i].curr_size = 0;
		tree->disk_blocks[i].hi = 0; 
		tree->disk_blocks[i].lo = 0; 
		tree->disk_blocks[i].nodes = NULL;

		// formulas for optimal bloom filter params from http://hur.st/bloomfilter?n=100&p=.05
		// num_bits = ceil((capacity * log(.05)) / log(1.0 / (pow(2.0, log(2.0)))));
		// tree->disk_blocks[i].num_bits = num_bits;
		// tree->disk_blocks[i].bloom_filter = malloc(ceil(num_bits / 8));
		// tree->disk_blocks[i].num_hashes = round(log(2.0) *  num_bits/ capacity);

		file = fopen(disk_names[i], "w");
		result = fclose(file);
		if (result) {
			perror("file close failed");
			return NULL;
		}
	}

	tree->num_written = 0;
	tree->maxlevels = maxlevels; 
	tree->blocksize = blocksize;

	return tree;
}

void lsm_destroy(lsm_tree *tree) { 
	// free nodes, then blocks, then tree 
	for (int i = 0; i < tree->maxlevels; i++) {
		free(tree->blocks[i].nodes);
	}
	free(tree->blocks); 
	free(tree->disk_blocks);
	free(tree);
}

void update_fence(block *block, int key) {
	int same = 0;
	if (block->curr_size == 0) {
		same = 1;
	}

	if (key < block->lo){
		block->lo = key; 
		if (same) {
			block->hi = key; 
		}
	}

	else if (key > block->hi){
		block->hi = key; 
		if (same) {
			block->lo = key;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//                                  PUT                                      //
///////////////////////////////////////////////////////////////////////////////

int put(int key, int value, char *strkey, lsm_tree *tree) {
	int result;
	node *newnodes = NULL;
	int i; 
	int j;
	int sum; 

	//fill a new node with the key/value
	node newnode; 
	newnode.key = key; 
	newnode.val = value;
	newnode.ghost = 0;

	mergesort(tree->blocks[0].nodes, tree->blocks[0].curr_size, sizeof(node), comparison);
 
	// first level is full so some merging must happen
	if (tree->blocks[0].curr_size == tree->blocks[0].capacity) {

		// at the very least we need to push to level 1
		i = 1; 

		// iterate to calculate what the highest level we must affect
		while(i < tree->maxlevels) {
			sum = tree->blocks[i].curr_size + tree->blocks[i - 1].curr_size;

			// if the sum does not exceed capacity, the correct level is level i
			if (sum <= tree->blocks[i].capacity)
				break;
			i++;
		}

		// if you must affec the disk levels, iterate to find the highest disk level 
		// we must affect 
		if (i == tree->maxlevels) {
			j = 0;
			if (tree->disk_blocks[0].curr_size == tree->disk_blocks[0].capacity) {
				j = 1; 
				while(j < tree->maxlevels) {
					sum = tree->disk_blocks[j].curr_size + tree->disk_blocks[j - 1].curr_size;
					if (sum <= tree->disk_blocks[j].capacity)
						break;
					j++;
					if (j == tree->maxlevels) {
						j = tree->maxlevels - 1;
						break;
					}
				}
			}
		}

		// if we must push to disk, push first before touching RAM buffers
		if (i == tree->maxlevels) {
			result = push_to_disk(tree, i);
			if (result != 0) {
				perror("Push to disk failed");
				return -1;
			}
			assert(tree->blocks[i - 1].curr_size == 0);
			i--;
		}


		// push changes throughout the tree
		while(i != 0) {
			mergesort(tree->blocks[i].nodes, tree->blocks[i].curr_size, sizeof(node), comparison);
			remove_duplicates(tree->blocks[i].nodes, tree->blocks[i].curr_size, tree->blocks[i - 1].nodes, tree->blocks[i - 1].curr_size);

			newnodes = malloc(sizeof(node) * tree->blocks[i].capacity);

			// merge level i and level (i - 1)
			merge_data(newnodes, tree->blocks[i].nodes, tree->blocks[i - 1].nodes, 
				tree->blocks[i].curr_size, tree->blocks[i - 1].curr_size); 
			memcpy(tree->blocks[i].nodes, newnodes, sizeof(node) * tree->blocks[i].capacity);
			// clear level (i - 1)
			tree->blocks[i].curr_size += tree->blocks[i - 1].curr_size;
			tree->blocks[i].lo = tree->blocks[i].nodes[0].key;
			tree->blocks[i].hi = tree->blocks[i].nodes[tree->blocks[i].curr_size - 1].key;
			tree->blocks[i - 1].curr_size = 0;
			//bf_refresh(&tree->blocks[i]);
			i--;
			free(newnodes);
		}
		//bf_refresh(&tree->blocks[0]);
	}

	// at this point we know there is enough space in level 0
	assert(tree->blocks[0].curr_size < tree->blocks[0].capacity);

	update_fence(&tree->blocks[0], key);
	// insert newnode
	tree->blocks[0].nodes[tree->blocks[0].curr_size] = newnode;
	tree->blocks[0].curr_size++;

	// update bloom filter
	//(strkey, tree->blocks[0].bloom_filter);
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

// read sz nodes from filename to buffer
node* disk_to_buffer(node *buffer, const char *filename, int sz) {
	FILE *file;
	file = fopen(filename, "r");
	buffer = malloc(sz * sizeof(node));
	if (!buffer) {
		perror("buffer mallocing failed");
		return NULL;
	}
	fread(buffer, sizeof(node), sz, file);
	fclose(file);
	return buffer;
}

// write sz nodes from buffer to filename
void buffer_to_disk(node *buffer, const char *filename, int sz) {
	FILE *file; 
	file = fopen(filename, "w");
	fwrite(buffer, sizeof(node), sz, file);
	fclose(file);
}

int push_to_disk(lsm_tree *tree,  int disk_level) { 
	node *all_data_buffer = NULL;
	node *disk_buffer = NULL;
	node *disk_buffer1 = NULL;
	node *disk_buffer2 = NULL;
	int curr_level = disk_level;
	int need_to_free = 1;

	while (curr_level != 0) {

		disk_buffer1 = disk_to_buffer(disk_buffer1, disk_names[curr_level], tree->disk_blocks[curr_level].curr_size);
		disk_buffer2 = disk_to_buffer(disk_buffer2, disk_names[curr_level  - 1], tree->disk_blocks[curr_level - 1].curr_size);
		
		all_data_buffer = malloc(sizeof(node) * (tree->disk_blocks[curr_level].curr_size + tree->disk_blocks[curr_level - 1].curr_size));
		if (!all_data_buffer) {
			perror("All data buffer malloc failed");
			return -1;
		}
		remove_duplicates(disk_buffer1, tree->disk_blocks[curr_level].curr_size, disk_buffer2, tree->disk_blocks[curr_level - 1].curr_size);
		merge_data(all_data_buffer, disk_buffer1, disk_buffer2, tree->disk_blocks[curr_level].curr_size, tree->disk_blocks[curr_level - 1].curr_size);

		buffer_to_disk(all_data_buffer, disk_names[curr_level], tree->disk_blocks[curr_level].curr_size + tree->disk_blocks[curr_level - 1].curr_size);

		tree->disk_blocks[curr_level].curr_size += tree->disk_blocks[curr_level - 1].curr_size;
		tree->disk_blocks[curr_level].lo = all_data_buffer[0].key;
		tree->disk_blocks[curr_level].hi = all_data_buffer[tree->disk_blocks[curr_level].curr_size - 1].key; 
		tree->disk_blocks[curr_level - 1].curr_size = 0;
		//bf_refresh(&tree->disk_blocks[curr_level]);
		curr_level--;

		free(disk_buffer1);
		free(disk_buffer2);
		free(all_data_buffer);
	}

	assert(curr_level == 0);

	disk_buffer = disk_to_buffer(disk_buffer, disk_names[0], tree->disk_blocks[0].curr_size);

	all_data_buffer = malloc(sizeof(node) * (tree->disk_blocks[0].curr_size+ tree->blocks[tree->maxlevels - 1].curr_size));
	if (!all_data_buffer) {
		perror("All data buffer malloc failed");
		return -1;
	}
	remove_duplicates(disk_buffer, tree->disk_blocks[0].curr_size, tree->blocks[tree->maxlevels - 1].nodes, tree->blocks[tree->maxlevels - 1].curr_size);

	// merge disk nodes with the lowest level of the lsm tree
	merge_data(all_data_buffer, disk_buffer, tree->blocks[tree->maxlevels - 1].nodes, 
		       tree->disk_blocks[0].curr_size, tree->blocks[tree->maxlevels - 1].curr_size);

	buffer_to_disk(all_data_buffer, disk_names[0], 
		tree->disk_blocks[0].curr_size + tree->blocks[tree->maxlevels - 1].curr_size);

	// bookkeeping 
	tree->disk_blocks[0].curr_size += tree->blocks[tree->maxlevels - 1].curr_size;
	tree->disk_blocks[0].lo = all_data_buffer[0].key;
	tree->disk_blocks[0].hi = all_data_buffer[tree->disk_blocks[0].curr_size - 1].key;
	tree->disk_blocks[tree->maxlevels - 1].curr_size = 0;
	//bf_refresh(&tree->disk_blocks[0]);

	free(disk_buffer);
	if (need_to_free)
		free(all_data_buffer);

	return 0; 
}

// merge two sorted buffers
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
int get(int key, char *strkey, int num_threads, lsm_tree *tree) {
	pthread_t tids[num_threads];
	(void)tids;
	int curr_level;

	// need a temporary storage location for the key to get
	node keynode;
	node *found; 
	keynode.key = key;
	node *disk_buffer = NULL;

	// sort the toplevel -- lowerlevels are sorted because of the push implementation
	mergesort(tree->blocks[0].nodes, tree->blocks[0].curr_size, sizeof(node), comparison);
	for (int i = 0; i < tree->maxlevels;i++) {
		// search for the node
		if (tree->blocks[i].curr_size != 0 && key <= tree->blocks[i].hi && key >= tree->blocks[i].lo) {
			// bloom filter search
			if (bf_search(strkey, tree->blocks[i].bloom_filter) == 0) {
				printf("\n");
				return -1;
			}

			found = bsearch(&keynode, tree->blocks[i].nodes, tree->blocks[i].curr_size, sizeof(node), comparison);
		    if (found) {
	    		printf("%d\n", found->val);
	    		return 0;
	    	}
	    }

	}

	curr_level = 0; 

	while(curr_level < tree->maxlevels) {

		assert(access(disk_names[curr_level], F_OK) != -1); 
		// check fence pointers
		if (tree->disk_blocks[curr_level].curr_size != 0 && key <= tree->disk_blocks[curr_level].hi 
			&& key >= tree->disk_blocks[curr_level].lo) {
			// did not find in RAM -- try search on disk
			disk_buffer = disk_to_buffer(disk_buffer, disk_names[curr_level], tree->disk_blocks[curr_level].curr_size);
			// search disk nodes
			if (bf_search(strkey, tree->disk_blocks[curr_level].bloom_filter) == 0) {
				printf("\n");
				return -1;
			}

			found = bsearch(&keynode, disk_buffer, tree->disk_blocks[curr_level].curr_size, sizeof(node), comparison);
		    if (found) {
		    	printf("%d\n", found->val);
		    	free(disk_buffer);
		    	return 0;
		    }
		    free(disk_buffer);
		}
		curr_level ++;
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
	int curr_level;

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
					foundlist = realloc(foundlist, sizeof(int) * num_found);
					foundlist[num_found - 1] = tree->blocks[i].nodes[j].key;
				}
			}
		}
	}

	// if we've found all the numbers, we are done. 
	if ((key2 - key1 + 1) == num_found) 
		return 0;

	curr_level = 0; 
	while(curr_level < tree->maxlevels) {
		// perhaps the rest are on disk!
		if (access("disk.txt", F_OK) != -1 && tree->disk_blocks[curr_level].curr_size != 0) {
			FILE *file = fopen("disk.txt", "r"); 
			if (!file) {
				perror("file open in get failed");
				free(foundlist);
				return -1;
			}

			// read in all nodes from disk
			node *disk_buffer = malloc(tree->disk_blocks[curr_level].curr_size * sizeof(node)); 
			if (!disk_buffer) {
				perror("Disk buffer malloc failed");
				free(foundlist);
				fclose(file);
				return -1;
			}

			result = fread(disk_buffer, sizeof(node), tree->disk_blocks[curr_level].curr_size, file); 

			result = fclose(file);
			if (result) {
				free(disk_buffer);
				free(foundlist);
				perror("File close failed in get");
				return -1;
			}	

			for (int i = 0; i < tree->disk_blocks[curr_level].curr_size; i++) {
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
		curr_level++;
	}
	free(foundlist);
	return 0;
}


///////////////////////////////////////////////////////////////////////////////
//                                DELETE                                     //
///////////////////////////////////////////////////////////////////////////////

int delete(int key, char *strkey, lsm_tree *tree) {
	int curr_level; 
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

	curr_level = 0;

	while(curr_level < tree->maxlevels) {
		// check if on disk
		if (access(disk_names[curr_level], F_OK) != -1 && tree->disk_blocks[curr_level].curr_size != 0) {
			FILE *file = fopen(disk_names[curr_level], "r"); 
			int result; 

			node *disk_buffer = malloc(tree->disk_blocks[curr_level].curr_size  * sizeof(node)); 
			if (!disk_buffer) {
				fclose(file); 
				perror("Disk buffer malloc failed");
				return -1;
			}

			// read in nodes from memory
			result = fread(disk_buffer, sizeof(node), tree->disk_blocks[curr_level].curr_size , file); 
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

			for (int i = 0; i < tree->disk_blocks[curr_level].curr_size ; i ++) {
				// found the key on disk! delete and return
				if (key == disk_buffer[i].key) {
					disk_buffer[i] = disk_buffer[tree->disk_blocks[curr_level].curr_size  - 1];
					tree->disk_blocks[curr_level].curr_size --;
					mergesort(disk_buffer, tree->disk_blocks[curr_level].curr_size , sizeof(node), comparison);

					file = fopen("disk.txt", "w");  
					result = fwrite(disk_buffer, sizeof(node), tree->disk_blocks[curr_level].curr_size , file);
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
		curr_level++;
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
		put(buffer[i], buffer[i+1], NULL, tree);
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


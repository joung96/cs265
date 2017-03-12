#include <stdio.h> 
#include <stdlib.h>
#include <assert.h> 
#include <unistd.h>  
#include "lsm.h"

///////////////////////////////////////////////////////////////////////////////
//                                  LSM                                      //
///////////////////////////////////////////////////////////////////////////////

lsm_tree *lsm_init(void) {
	lsm_tree *tree;
	tree = malloc(sizeof(lsm_tree));
	if (!tree) {
		perror("Initial tree malloc failed");
		return  NULL;
	}

	tree->capacity =  BLOCKSIZE;
	tree->curr_size = 0; 
	tree->block = malloc(sizeof(node*) * BLOCKSIZE);
	tree->num_written = 0;
	tree->is_sorted = 0;
	if (!tree->block) 
		perror("Initial block malloc failed");
	return tree;
}

void lsm_destroy(lsm_tree *tree) { 
	free(tree->block); 
	free(tree);
}

///////////////////////////////////////////////////////////////////////////////
//                                  PUT                                      //
///////////////////////////////////////////////////////////////////////////////

int put(int key, int value, lsm_tree *tree) {
	int result;
	//check if block is full 
	node newnode; 
	newnode.key = key; 
	newnode.val = value;

	if (tree->capacity == tree->curr_size) {
		// push 
		result = push_to_disk(tree);
		if (result != 0) {
			perror("Push to disk failed");
			return -1;
		}
	}

	// insert into level 1
	tree->block[tree->curr_size] = newnode;

	tree->curr_size++; 
	tree->is_sorted = 0;
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
	qsort(tree->block, tree->curr_size, sizeof(node), comparison);
	tree->is_sorted = 1;

	node *all_data_buffer = NULL;
	node *disk_buffer = NULL;
	FILE *file = NULL;
	int result = 0;


	// file exists
	if (access("disk.txt", F_OK) != -1) {
		file = fopen("disk.txt", "r");

		disk_buffer = malloc(tree->num_written * sizeof(node)); 
		if (!disk_buffer) {
			fclose(file); 
			perror("Disk buffer malloc failed");
			return -1;
		}

		result = fread(disk_buffer, sizeof(node), tree->num_written, file); 
		if (result == 0) {
			fclose(file); 
			perror("Read into buffer failed");
			return -1;
		}

		all_data_buffer = malloc(sizeof(node) * (tree->num_written + tree->curr_size));
		if (!all_data_buffer) {
			fclose(file); 
			perror("All data buffer malloc failed");
			return -1;
		}

		merge_data(all_data_buffer, disk_buffer, tree->block, tree->num_written, tree->curr_size);
		free(disk_buffer);

		result = fclose(file); 
		if (result) {
			perror("file close failed 1"); 
			return -1;
		}
	}
	// file DNE 
	file = fopen("disk.txt", "w");  
	if (! all_data_buffer) {
		all_data_buffer = tree->block;
	}
	result = fwrite(all_data_buffer, sizeof(node), (tree->num_written+tree->curr_size), file);
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

	if(tree->num_written)
		free(all_data_buffer); 
	
	tree->num_written += tree->curr_size;
	tree->curr_size = 0; 
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
	qsort(tree->block, tree->curr_size, sizeof(node), comparison);
	tree->is_sorted = 1;

	int result; 

	node keynode; 
	keynode.key = key;

    node *found = bsearch(&keynode, tree->block, tree->curr_size, sizeof(node), comparison);
    if (found) {
    	printf("%d\n", found->val);
    	return 0;
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
			perror("File close failed in get");
			return -1;
		}	

		found = bsearch(&keynode, disk_buffer, tree->num_written, sizeof(node), comparison);
	    if (found) {
	    	printf("%d\n", found->val);
	    	return 0;
	    }


	} 
	// not found
	printf("\n");
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
//                                RANGE                                      //
///////////////////////////////////////////////////////////////////////////////

void range(int key1, int key2, lsm_tree *tree){
	int none_found = 1;
	for (int i = 0; i < tree->curr_size; i ++) {
		if (key1 <= tree->block[i].key && tree->block[i].key < key2) {
			none_found = 0;
			printf("%d:%d\n", tree->block[i].key, tree->block[i].val);
		}
		if (tree->block[i].key < key2){
			printf("\n");
			return;
		}
	}
	if (none_found)
		printf("\n");
}


///////////////////////////////////////////////////////////////////////////////
//                                DELETE                                     //
///////////////////////////////////////////////////////////////////////////////

int delete(int key, lsm_tree *tree) {
	// in memory
	for (int i = 0; i < tree->curr_size; i ++) {
		if (key == tree->block[i].key) {
			tree->block[i] = tree->block[tree->curr_size - 1];
			tree->curr_size--;
			tree->is_sorted = 0;
			return 0;
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
			perror("fread failed while deleting");
			return -1;
		}	

		result = fclose(file); 
		if (result) {
			perror("file close failed 2"); 
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
					perror("fwrite failed while writing to disk 1");
					return -1;	
				}

				result = fclose(file); 
				if (result) {
					perror("file close failed 2"); 
					return -1;
				}
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
///////////////////////////////////////////////////////////////////////////////
int stat(lsm_tree *tree) {
	int total_pairs = 0; 
	int level1 = 0; 

	// gather statistics 
	for (int i = 0; i < tree->curr_size; i ++) {
		total_pairs++;
		level1++;
	}

	// print statistics
	printf("Total Pairs: %d\n", total_pairs);
	printf("LV1: %d\n", level1);
	for (int i = 0; i < tree->curr_size; i ++) {
		printf("%d:%d:L1 ", tree->block[i].key, tree->block[i].val);
	}
	return 0;

}












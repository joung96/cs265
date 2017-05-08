#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <assert.h>
#include <string.h>
#include "lsm.h"

int successful_gets = 0;
int failed_gets = 0; 

/* arguments to lsm tree
   1) blocksize
   2) multiplier
   3) maxlevels 
   4) num threads
   5) workload filename
*/ 
int main(int argc, char *argv[]) {
	// clock_t begin; 
	// clock_t end; 
	// struct timeval time_begin, time_end;
	// double clock_spent;
	// double time_spent;
	char type;
	char strkey[10];
	int value;
	int hi = 0;
	int lo = 0;

	int blocksize = atoi(argv[1]); 
	int multiplier = atoi(argv[2]); 
	int maxlevels = atoi(argv[3]);

	printf("BLOCKSIZE: %d, MULTIPLIER: %d, MAXLEVELS: %d\n", blocksize, multiplier, maxlevels);
	lsm_tree *tree = lsm_init(blocksize, multiplier, maxlevels);
	stat(tree);
	int result; 
	while(1) {
		printf("What type of operation would you like to do?: ");
		while(scanf("%c", &type) == 1) {
			if (type != '\n')
				break;
		};
		switch (type) {
			case 'p': 
				printf("Enter the key value pair you would like to input: ");
				scanf("%s %d", strkey, &value);
				result = put(atoi(strkey), value, strkey, tree);
				if (result < 0 ) {
					perror("an error has occurred\n");
					return -1; 
				}
				stat(tree);
				break;
			case 'g': 
				printf("Enter the key you would like to get: ");
				scanf("%s", strkey);
				result = get(atoi(strkey), strkey, tree);
				if (result < 0) {
					printf("key not found :(\n");
				}
				break;
			case 'd': 
				printf("Enter the key you would like to delete: ");
				scanf("%s", strkey);
				result = delete(atoi(strkey), strkey, tree);
				if (result < 0 ) {
					printf("delete failed!\n");
				}
				stat(tree);
				break;
			case 'r': 
				printf("Enter the low and high bounds for the range query: ");
				scanf("%d %d", &lo, &hi);
				result = range(lo, hi, tree);
				if (result < 0 ) {
					printf("range failed :( \n");
				}
				break;
			case 'l': 
				printf("Enter the filename: ");
				scanf("%s", strkey);
				load(strkey, tree);
				stat(tree);
				break;
			case 'q': 
				printf("Here is your final tree:\n");
				stat(tree); 
				printf("BYE!\n");
				return 0;
				break;
		}
		
	}

	return 0;
}
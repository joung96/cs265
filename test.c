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
	clock_t begin; 
	clock_t end; 
	struct timeval time_begin, time_end;
	double clock_spent;
	double time_spent;
	int ranges = 0; 
	int successul_deletes = 0; 
	int failed_deletes = 0; 
	int loads = 0; 
	int total_gets = 0;
	int puts = 0;

	int blocksize = atoi(argv[1]); 
	int multiplier = atoi(argv[2]); 
	int maxlevels = atoi(argv[3]);

	lsm_tree *tree = lsm_init(blocksize, multiplier, maxlevels);
	int result; 

	const char *filename = argv[4]; 
	FILE *file = fopen(filename, "r"); // check res
	if (!file) {
		perror("File was NULL");
		return -1;
	}
	char line[50]; 
	char *token; 
	char *loadfile;
	char *strkey;
	int key; 
	int val;

	//for every line
  	gettimeofday(&time_begin, NULL);
	begin = clock();
	while(fgets(line, sizeof(line), file)) {
		token = strtok(line, " "); 
		switch (*token) {
			case 'p': 
				strkey = strtok(NULL, " ");
				key = atoi(strkey);
				val = atoi(strtok(NULL, " "));
				result = put(key, val, strkey, tree);
				if (result >= 0)
					puts++; 
				else {
					return -1;
				}
				break;

			case 'g': 
				total_gets++;
				strkey = strtok(NULL, " ");
				key = atoi(strkey);
				get(key, strkey, 0, tree);
				break;

			case 'd': 
				strkey = strtok(NULL, " ");
				key = atoi(strkey);
				result = delete(key, strkey, tree);
				if (result) 
					failed_deletes++;
				else 
					successul_deletes++;
				break;

			case 'r': 
				key = atoi(strtok(NULL, " "));
				val = atoi(strtok(NULL, " "));
				range(key, val, tree);
				ranges++;
				break;

			case 'l': 
				loadfile = strtok(NULL, " ");
				int i = 0 ;
				while(loadfile[i + 1] != '.')
					i++;
				char filebuffer[i + 5]; 
				for (int j = 0; j < i; j++) 
					filebuffer[j] = loadfile[j + 1];
				filebuffer[i] = '.'; 
				filebuffer[i + 1] = 'd';
				filebuffer[i + 2] = 'a';
				filebuffer[i + 3] = 't';
				filebuffer[i + 4] = '\0';
		    	load(filebuffer, tree);
				loads++;
				break;
		}
	}
	gettimeofday(&time_end, NULL);
	end = clock();
	clock_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    time_spent = (double) (time_end.tv_usec - time_begin.tv_usec) /1000000 + (double)(time_end.tv_sec - time_begin.tv_sec);
	printf("Took %f clocks and %f seconds\n", clock_spent, time_spent);

	fclose(file);

	// printf("------------------------------------\n");
	// printf("PUTS %d\n", puts);
	// printf("TOTAL GETS %d\n", total_gets);
	// printf("SUCCESSFUL GETS %d\n", successful_gets);
	// printf("FAILED GETS %d\n", failed_gets);
	// printf("RANGES %d\n", ranges);
	// printf("SUCCESSFUL_DELS %d\n", successul_deletes);
	// printf("FAILED_DELS %d\n", failed_deletes);
	// printf("LOADS %d\n", loads);
	return 0;
}
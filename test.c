#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include "lsm.h"

/* arguments to lsm tree
   1) blocksize
   2) multiplier
   3) maxlevels 
   4) num threads
   5) workload filename
*/ 
int main(int argc, char *argv[]) {
	int blocksize = atoi(argv[1]); 
	int multiplier = atoi(argv[2]); 
	int maxlevels = atoi(argv[3]);
	int num_threads = atoi(argv[4]);

	lsm_tree *tree = lsm_init(blocksize, multiplier, maxlevels);
	int result; 

	const char *filename = argv[5]; 
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

	int puts = 0;
	int successful_gets = 0;
	int failed_gets = 0; 
	int ranges = 0; 
	int successul_deletes = 0; 
	int failed_deletes = 0; 
	int loads = 0; 
	int total_gets = 0;

	// for every line
   
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
					printf("FAILED PUT!");
					return -1;
				}
				break;

			case 'g': 
				total_gets++;
				strkey = strtok(NULL, " ");
				key = atoi(strkey);
				if (get(key, strkey, num_threads, tree) == 0) 
					successful_gets++;
				else {
					printf("FAILED: %d", key);
					failed_gets++;
				}
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

	fclose(file);

	printf("------------------------------------\n");
	printf("PUTS %d\n", puts);
	printf("TOTAL GETS %d\n", total_gets);
	printf("SUCCESSFUL GETS %d\n", successful_gets);
	printf("FAILED GETS %d\n", failed_gets);
	printf("RANGES %d\n", ranges);
	printf("SUCCESSFUL_DELS %d\n", successul_deletes);
	printf("FAILED_DELS %d\n", failed_deletes);
	printf("LOADS %d\n", loads);
	return 0;
}
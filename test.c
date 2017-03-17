#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include "lsm.h"


int main(int argc, char *argv[]) {

	lsm_tree *tree = lsm_init();
	int result; 

	const char *filename = argv[1]; 
	FILE *file = fopen(filename, "r"); // check res
	if (!file) {
		perror("File was NULL");
		return -1;
	}
	char line[50]; 
	char *token; 
	char *loadfile;
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
	clock_t t = clock();
   
	while(fgets(line, sizeof(line), file)) {
		token = strtok(line, " "); 
		switch (*token) {
			case 'p': 
				key = atoi(strtok(NULL, " "));
				val = atoi(strtok(NULL, " "));
				result = put(key, val, tree);
				if (result >= 0)
					puts++; 
				else {
					printf("FAILED PUT!");
					return -1;
				}
				break;

			case 'g': 
				total_gets++;
				key = atoi(strtok(NULL, " "));
				if (get(key, tree) == 0) 
					successful_gets++;
				else 
					failed_gets++;
				break;

			case 'd': 
				key = atoi(strtok(NULL, " "));
				result = delete(key, tree);
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
				load(loadfile, tree);
				loads++;
				break;
		}
	}
	double time_elapsed = (clock() - t) / CLOCKS_PER_SEC;

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
	printf("TIME_ELAPSED %lf\n", time_elapsed);

	return 0;
}
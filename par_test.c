#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
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
	int successful_deletes = 0; 
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
		perror("File was NULL1");
		return -1;
	}
	const char *getfile = argv[5];
	FILE *gets = fopen(getfile, "r");
	if (!gets) {
		perror("File was NULL2");
		return -1;
	}
	int num_threads = 6;
	char line[50]; 
	char *token; 
	char *strkey;
	int *keys = malloc(sizeof(int) * MAX_GETS);
	int i;
	int j;
	int key; 
	int val;

	//for every line
  	gettimeofday(&time_begin, NULL);
	begin = clock();
	while(fgets(line, sizeof(line), file)) {
		token = strtok(line, " "); 
		if (*token == 'p') {
			strkey = strtok(NULL, " ");
			key = atoi(strkey);
			keys[puts] = key;
			val = atoi(strtok(NULL, " "));
			result = put(key, val, strkey, tree);
			if (result >= 0)
				puts++; 
			else {
				return -1;
			}
		}
		if (puts % 100000 == 0) {
			printf("%d\n", puts);
		}
	}
	gettimeofday(&time_end, NULL);
	end = clock();
	clock_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    time_spent = (double) (time_end.tv_usec - time_begin.tv_usec) /1000000 + (double)(time_end.tv_sec - time_begin.tv_sec);
	// printf("Put with used %f clocks and %f seconds\n", clock_spent, time_spent);
	// return 0;

	//load data
	while(fgets(line, sizeof(line), gets)) {
		token = strtok(line, " "); 
		strkey = strtok(NULL, " ");
		key = atoi(strkey);
		keys[total_gets] = key;
		assert(*token == 'g');
		total_gets++;
	}

	for (j = 1; j < num_threads; j ++) {
		gettimeofday(&time_begin, NULL);
		begin = clock();
		pthread_t tids[j];
		thread_args *inputs = malloc(sizeof(thread_args) * j);
		int thread_load = puts / j;

		for (i = 0; i < j; i ++) {
			if (i == (j - 1)) {
				inputs[i].load_sz = puts - thread_load * i;
			}
			inputs[i].load_sz = thread_load;
			inputs[i].start_index = i * thread_load;
			inputs[i].load_sz = thread_load;
			inputs[i].allkeys = keys;
			inputs[i].tree = tree;
			pthread_create(&tids[i], NULL, &thread_get, &inputs[i]);
		}
		for(i = 0 ; i < j; i++)
	        pthread_join(tids[i], NULL); 
	    free(inputs);
	    end = clock();
	    gettimeofday(&time_end, NULL);
	    clock_spent = (double)(end - begin) / CLOCKS_PER_SEC;
	    time_spent = (double) (time_end.tv_usec - time_begin.tv_usec) /1000000 + (double)(time_end.tv_sec - time_begin.tv_sec);
		printf("Parallel GET with %d threads used %f clocks and %f seconds\n",j, clock_spent, time_spent);
		sync();
	}

	(void)successful_gets; 
	(void)failed_gets;
	(void)ranges;
	(void)successful_deletes;
	(void)failed_deletes;
	(void)loads;
	return 0;
}

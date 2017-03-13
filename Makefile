all:
	touch disk.txt
	rm disk.txt
	# rm -r lsm.DSYM
	gcc lsm.c test.c -o lsm -Wall -g

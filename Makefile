all:
	touch disk0.txt
	touch disk1.txt
	touch disk2.txt
	touch disk3.txt
	touch disk4.txt
	rm disk0.txt 
	rm disk1.txt 
	rm disk2.txt 
	rm disk3.txt 
	rm disk4.txt 
	 
	gcc lsm.c test.c -o lsm -Wall -g

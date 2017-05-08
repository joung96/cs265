general:
	touch disk0.txt
	touch disk1.txt
	touch disk2.txt
	touch disk3.txt
	touch disk4.txt
	touch disk5.txt
	touch disk6.txt
	touch disk7.txt
	touch disk8.txt
	touch disk9.txt
	rm disk0.txt 
	rm disk1.txt 
	rm disk2.txt 
	rm disk3.txt 
	rm disk4.txt 
	rm disk5.txt
	rm disk6.txt
	rm disk7.txt
	rm disk8.txt
	rm disk9.txt
	 
	gcc bloom.c lsm.c test.c -o lsm -Wall -g

parallel: 
	touch disk0.txt
	touch disk1.txt
	touch disk2.txt
	touch disk3.txt
	touch disk4.txt
	touch disk5.txt
	touch disk6.txt
	touch disk7.txt
	touch disk8.txt
	touch disk9.txt
	rm disk0.txt 
	rm disk1.txt 
	rm disk2.txt 
	rm disk3.txt 
	rm disk4.txt 
	rm disk5.txt
	rm disk6.txt
	rm disk7.txt
	rm disk8.txt
	rm disk9.txt
	 
	gcc bloom.c lsm.c par_test.c -o lsm -Wall -g

demo:
	touch disk0.txt
	touch disk1.txt
	touch disk2.txt
	touch disk3.txt
	touch disk4.txt
	touch disk5.txt
	touch disk6.txt
	touch disk7.txt
	touch disk8.txt
	touch disk9.txt
	rm disk0.txt 
	rm disk1.txt 
	rm disk2.txt 
	rm disk3.txt 
	rm disk4.txt 
	rm disk5.txt
	rm disk6.txt
	rm disk7.txt
	rm disk8.txt
	rm disk9.txt
	 
	gcc bloom.c lsm.c demo.c -o lsm -Wall -g


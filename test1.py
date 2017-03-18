from subprocess import call
import time

blocksizes = ['1', '10', '100', '1000', '10000']
multipliers = ['1','2','3'];
maxlevels = ['1', '2', '3', '4', '5'];

f = open('dump.txt', 'w')

for blocksize in blocksizes: 
	for multiplier in multipliers: 
	    for maxlevel in maxlevels: 
			timelist = [] 
			print "BLOCKSIZE:%s MULTIPLIER:%s, MAXLEVEL:%s\n" % (blocksize, multiplier, maxlevel)
			for i in range(50):
				start = time.time()
				call(["./lsm", blocksize, multiplier, maxlevel, 'generator/workload1.txt'])
				call(["make"], stdout=f)
				end = time.time() 
				timelist.append(end-start)
			print sum(timelist)/50


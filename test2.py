from subprocess import call
import time

blocksizes = ['1', '10', '100', '1000', '10000']
multipliers = ['1','2','3', '4', '5']
maxlevels = ['1', '2', '3', '4', '5']

f = open('dump.txt', 'w')

for blocksize in blocksizes: 
	for multiplier in multipliers: 
	    for maxlevel in maxlevels: 
			timelist = [] 
			print "BLOCKSIZE:%s MULTIPLIER:%s, MAXLEVEL:%s\n" % (blocksize, multiplier, maxlevel)
			start = time.time()
			call(["make"], stdout=f)
			call(["./lsm", blocksize, multiplier, maxlevel, "work.txt"])
			end = time.time() 
			timelist.append(end-start)
			print sum(timelist)/50


#!/usr/bin/env python
import csv
import os

from collections import defaultdict
from sets import Set

datafile1 = os.path.join(os.path.dirname(__file__), '../output/raw/8b.csv')
datafile2 = os.path.join(os.path.dirname(__file__), '../output/raw/tpcc-results-cavalia_b.csv')
outfile = os.path.join(os.path.dirname(__file__), '../output/average/8b.csv')

dataseries = ['MV3C','OMVCC','OCC','SILO']
alldata = defaultdict(lambda : dict(map(lambda d: (d, []), dataseries)))

def aggregate(l):
	#print l
	n = len(l)
	if(n != 10):
		print("Count should be 10, instead it is " + str(n))
	#return (l[-1] + l[-2] + l[-3])/3 #last 3
	#return (l[-1] + l[-2] + l[-3] + l[-4] + l[-5])/5 #last 5
	#return sum(l)/len(l)	#average
	l.sort(reverse=True)
	return (l[0]+l[1]+l[2])/3 #best 3
	#return (l[0]+l[1]+l[2]+l[3]+l[4])/5 #best 5	

with open(datafile1, 'r') as csvfile:
	reader = csv.DictReader(csvfile)
	for row in reader:
		d = row['Algo']
		x = int(row['NumWare'])
		p = row['Throughput(ktps)']
		alldata[x][d].append(float(p))

with open(datafile2, 'r') as csvfile:
	reader = csv.DictReader(csvfile)
	for row in reader:
		d = row[' Algo']
		x = int(row[' NumWare'])
		p = row[' Throughput(ktps)']
		alldata[x][d].append(float(p))		

with open(outfile, 'w') as csvfile:
	writer = csv.writer(csvfile)
	writer.writerow([','] + dataseries)
	for x in sorted(alldata.keys()):
		row = [x]
		for d in dataseries:
			row.append(aggregate(alldata[x][d]))
		writer.writerow(row)	

#!/usr/bin/env python
import csv
import os

from collections import defaultdict
from sets import Set

datafile = os.path.join(os.path.dirname(__file__), '../output/raw/7b.csv')
outfile = os.path.join(os.path.dirname(__file__), '../output/average/7b.csv')

dataseries = ['MV3C','OMVCC']
alldata = defaultdict(lambda : dict(map(lambda d: (d, []), dataseries)))

def aggregate(l):
	#print l
	#return (l[-1] + l[-2] + l[-3])/3 #last 3
	#return (l[-1] + l[-2] + l[-3] + l[-4] + l[-5])/5 #last 5
	#return sum(l)/len(l)	#average
	l.sort(reverse=True)
	return (l[0]+l[1]+l[2])/3 #best 3
	#return (l[0]+l[1]+l[2]+l[3]+l[4])/5 #best 5

with open(datafile, 'r') as csvfile:
	reader = csv.DictReader(csvfile)
	for row in reader:
		d = row['Algo']
		x = int(float(row['ConflictFraction'])*100)
		p = row['Throughput(ktps)']
		alldata[x][d].append(float(p))

with open(outfile, 'w') as csvfile:
	writer = csv.writer(csvfile)
	writer.writerow([','] + dataseries)
	for x in sorted(alldata.keys()):
		row = [str(x)+ '%']
		for d in dataseries:
			row.append(aggregate(alldata[x][d]))
		writer.writerow(row)

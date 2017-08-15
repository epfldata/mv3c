#!/usr/bin/env python
import csv
import os

from collections import defaultdict
from sets import Set

datafile = os.path.join(os.path.dirname(__file__), '../output/raw/7a.csv')
outfile = os.path.join(os.path.dirname(__file__), '../output/average/7a.csv')
outdir = os.path.dirname(outfile)
try:
    os.stat(outdir)
except:
    os.mkdir(outdir)
dataseries = ['MV3C','OMVCC']
alldata = defaultdict(lambda : dict(map(lambda d: (d, []), dataseries)))

def aggregate(l):
	l.sort(reverse=True)
	if(len(l) >= 3):
		return (l[0]+l[1]+l[2])/3 #best 3
	elif(len(l) == 2):
		return (l[0]+l[1])/2
	else: 
		return l[0]

with open(datafile, 'r') as csvfile:
	reader = csv.DictReader(csvfile)
	for row in reader:
		d = row['Algo']
		x = int(row['NumThreads'])
		p = row['Throughput(ktps)']		
		alldata[x][d].append(float(p))

with open(outfile, 'w') as csvfile:
	writer = csv.writer(csvfile)
	writer.writerow([','] + dataseries)
	for x in sorted(alldata.keys()):
		row = [x]
		for d in dataseries:
			row.append(aggregate(alldata[x][d]))
		writer.writerow(row)

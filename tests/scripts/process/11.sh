#!/usr/bin/env python
import csv
import os

from collections import defaultdict
from sets import Set

datafile1 = os.path.join(os.path.dirname(__file__), '../output/raw/11-omvcc.csv')
datafile2 = os.path.join(os.path.dirname(__file__), '../output/raw/11-mv3cWW.csv')
outfile = os.path.join(os.path.dirname(__file__), '../output/average/11.csv')
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

with open(datafile1, 'r') as csvfile:
	reader = csv.DictReader(csvfile)
	for row in reader:
		d = 'OMVCC'
		x = int(row['WindowSize'])
		if(x>=10):
			p = float(row['Commits'])/float(row['Time'])
			alldata[x][d].append(p)

with open(datafile2, 'r') as csvfile:
	reader = csv.DictReader(csvfile)
	for row in reader:
		d = 'MV3C'
		x = int(row['WindowSize'])
		if(x>=10):
			p = float(row['Commits'])/float(row['Time'])
			alldata[x][d].append(p)
		
with open(outfile, 'w') as csvfile:
	writer = csv.writer(csvfile)
	writer.writerow([','] + dataseries)
	for x in sorted(alldata.keys()):
		row = [x]
		for d in dataseries:
			row.append(aggregate(alldata[x][d]))
		writer.writerow(row)

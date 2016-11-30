import csv
from collections import defaultdict
from sets import Set
name = '8a1'
dataseries = ['MV3C','OMVCC','OCC','SILO']
alldata1 = defaultdict(lambda : dict(map(lambda d: (d, []), dataseries)))
alldata2 = defaultdict(lambda : dict(map(lambda d: (d, []), dataseries)))
def aggregate(l,x,d):
	n = len(l)
	if(n != 10):
		print("Count should be 10, instead it is " + str(n) + "for xpoint " +str(x)+ " and dataseries " + d)
	#print l
	#return (l[-1] + l[-2] + l[-3])/3 #last 3
	#return (l[-1] + l[-2] + l[-3] + l[-4] + l[-5])/5 #last 5
	#return sum(l)/len(l)	#average
	l.sort(reverse=True)
	return (l[0]+l[1]+l[2])/3 #best 3
	#return (l[0]+l[1]+l[2]+l[3]+l[4])/5 #best 5	

with open(name+'.csv', 'r') as csvfile:
	reader = csv.DictReader(csvfile)
	for row in reader:
		d = row['Algo']
		x = int(row['NumThreads'])
		p = row['Throughput(ktps)']		
		alldata1[x][d].append(float(p))
with open('tpcc-results-cavalia_a.csv', 'r') as csvfile:
	reader = csv.DictReader(csvfile)
	for row in reader:
		d = row[' Algo']
		x = int(row[' NumThreads'])
		p = row[' Throughput(ktps)']
		if(int(row[' NumWare']) == 1):
		 alldata1[x][d].append(float(p))	

with open(name+'-thr.avg', 'w') as csvfile:
	writer = csv.writer(csvfile)
	writer.writerow([','] + dataseries)
	for x in sorted(alldata1.keys()):
		row = [x]
		for d in dataseries:
			row.append(aggregate(alldata1[x][d],x,d))
		writer.writerow(row)

name = '8a2'		
with open(name+'.csv', 'r') as csvfile:
	reader = csv.DictReader(csvfile)
	for row in reader:
		d = row['Algo']
		x = int(row['NumThreads'])
		p = row['Throughput(ktps)']		
		alldata2[x][d].append(float(p))
with open('tpcc-results-cavalia_a.csv', 'r') as csvfile:
	reader = csv.DictReader(csvfile)
	for row in reader:
		d = row[' Algo']
		x = int(row[' NumThreads'])
		p = row[' Throughput(ktps)']
		if(int(row[' NumWare']) == 2):
		 alldata2[x][d].append(float(p))		
with open(name+'-thr.avg', 'w') as csvfile:
	writer = csv.writer(csvfile)
	writer.writerow([','] + dataseries)
	for x in sorted(alldata2.keys()):
		row = [x]
		for d in dataseries:
			row.append(aggregate(alldata2[x][d],x,d))
		writer.writerow(row)
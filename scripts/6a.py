import csv
from collections import defaultdict
from sets import Set
name = '6a'
dataseries = ['MV3C','OMVCC']
alldata = defaultdict(lambda : dict(map(lambda d: (d, []), dataseries)))
#alldata = dict(map(lambda x: (x,defaultdict(list)), dataseries))
#xpoints = Set()
def aggregate(l):
	print l
	return sum(l)/len(l)	
with open(name+'.csv', 'r') as csvfile:
	reader = csv.DictReader(csvfile)
	for row in reader:
		d = row['Algo']
		x = int(row['NumThreads'])
		p = row['ScaledTime']
		#if(row['Critical Compensate'] != 'N'):
		alldata[x][d].append(float(p))
		#xpoints.add(fee)
with open(name+'.avg', 'w') as csvfile:
	writer = csv.writer(csvfile)
	writer.writerow([','] + dataseries)
	for x in sorted(alldata.keys()):
		row = [x]
		for d in dataseries:
			row.append(aggregate(alldata[x][d]))
		writer.writerow(row)

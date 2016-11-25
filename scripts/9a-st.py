import csv
from collections import defaultdict
from sets import Set
name = '9a'
dataseries = ['MV3C','OMVCC']
alldata = defaultdict(lambda : dict(map(lambda d: (d, []), dataseries)))

def aggregate(l):
	#print l
	#return (l[-1] + l[-2] + l[-3])/3 #last 3
	#return (l[-1] + l[-2] + l[-3] + l[-4] + l[-5])/5 #last 5
	#return sum(l)/len(l)	#average
	l.sort()
	return (l[0]+l[1]+l[2])/3 #best 3
	#return (l[0]+l[1]+l[2]+l[3]+l[4])/5 #best 5	

with open(name+'.csv', 'r') as csvfile:
	reader = csv.DictReader(csvfile)
	for row in reader:
		d = row['Algo']
		x = int(row['NumThreads'])
		p = row['ScaledTime']		
		alldata[x][d].append(float(p)/1000)
		
with open(name+'-st.avg', 'w') as csvfile:
	writer = csv.writer(csvfile)
	writer.writerow([','] + dataseries)
	for x in sorted(alldata.keys()):
		row = [x]
		for d in dataseries:
			row.append(aggregate(alldata[x][d]))
		writer.writerow(row)

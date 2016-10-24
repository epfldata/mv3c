#!/bin/bash
rm -rf *TPCC*.out
rm -f 9b*.csv
rm -f header out
echo "TPCC  conflict test "
numThr=10
for p in 1 2 3 4 5 6 7 8 10 12 14 16 18 20
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 ./compileTPCC.sh


for i in {1..5}
do
for p in 1 2 3 4 5 6 7 8 10 12 14 16 18 20
do
./mvccTPCC-$numThr-$p.out	
./mv3cTPCC-$numThr-$p.out		
done
done


cat header out > 9b.csv
rm -f header out

#!/bin/bash
rm -rf *TPCC*.out
rm -f 9a*.csv
rm -f header out
echo "TPCC  thread test "

for p in 1 2
do
for numThr in {1..10}
do
echo -n "$numThr $p " 
done
done | xargs  -n 2 -P10 ./compileTPCC.sh


for i in {1..5}
do
for p in 1 2
do
for numThr in  {1..10}
do
./mvccTPCC-$numThr-$p.out	
./mv3cTPCC-$numThr-$p.out		
done
done
done

cat header out > 9a.csv
rm -f header out

#!/bin/bash
rm -rf *TPCC*.out
rm -f 9*.csv
rm -f header out
echo "TPCC  combined test Transactions=$num"

for p in 1 2  6 10 16 20
do
for numThr in  1 3 6 10
do
echo -n "$numThr $p " 
done
done | xargs  -n 2 -P10 ./compileTPCC.sh


for i in 1 2 3
do
for p in 1 2 6 10 16 20
do
for numThr in  1 3 6 10
do
./mvccTPCC-$numThr-$p.out	
./mvccTPCC-Cuckoo-$numThr-$p.out	
./mv3cTPCC-$numThr-$p.out	
./mv3cTPCC-Cuckoo-$numThr-$p.out	
./mv3cTPCC-Crit-$numThr-$p.out	
./mv3cTPCC-Crit-Cuckoo-$numThr-$p.out	
done
done
done

cat header out > 9.csv
#rm -f header out

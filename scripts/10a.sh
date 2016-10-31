#!/bin/bash
rm -rf *TPCC2*.out
rm -f 10a*.csv
rm -f header out
echo "TPCC_2 thread test "

for p in 5
do
for numThr in {1..12}
do
echo -n "$numThr $p " 
done
done | xargs  -n 2 -P10 ./compileTPCC2.sh

for i in 1 2 3
do
for p in 5 
do
for numThr in  {1..12}
do
./mvccTPCC2-$numThr-$p.out	
./mv3cTPCC2-$numThr-$p.out		
done
done
done

cat header out > 10a.csv
rm -f header out

#!/bin/bash

rm -rf *Trading*.out
rm -f 6a*.csv
rm -f header out
echo "Trading thread test"
p=1.4
for numThr in {1..10}
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 ./compileTrading.sh

for i in {1..5}
do
for numThr in {1..10}
do
./mvccTrading-$numThr-$p.out	
./mv3cTrading-$numThr-$p.out	
done
done

cat header out > 6a.csv

rm -f header out




#!/bin/bash

rm -rf *Banking*.out
rm -f 7a*.csv
rm -f header out
echo "Banking  thread test"
p=1.0
for numThr in  {1..10}
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 ./compileBanking.sh

for i in {1..5}
do
for numThr in  {1..10}
do
./mvccBanking-$numThr-$p.out
./mv3cBanking-$numThr-$p.out
./mv3cBankingCrit-$numThr-$p.out	
done
done

cat header out > 7a.csv
rm -f header out

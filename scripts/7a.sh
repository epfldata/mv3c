#!/bin/bash

rm -rf *Banking*.out
rm -f 7a*.csv
rm -f header out
echo "Banking  thread test Transactions"
p=1.0
for numThr in  {1..10}
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 ./compileBanking.sh

for i in {1..3}
do
for numThr in  {1..10}
do
./mvccBanking-$numThr-$p.out	
done
done

cat header out > 7a-omvcc.csv
rm -f header out


for i in {1..3}
do
for numThr in  {1..10}
do
./mv3cBanking-$numThr-$p.out	
done
done
cat header out > 7a-mv3c.csv
rm -f header out

for i in {1..3}
do
for numThr in  {1..10}
do
./mv3cBankingCrit-$numThr-$p.out	
done
done
cat header out > 7a-mv3cCrit.csv
rm -f header out

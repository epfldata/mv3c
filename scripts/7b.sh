#!/bin/bash

rm -rf *Banking*.out
rm -f 7b*.csv
rm -f header out
echo "Banking  conflict test Transactions"
for p in 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0
do
for numThr in  5 10
do
echo -n "$numThr $p " 
done
done | xargs  -n 2 -P10 ./compileBanking.sh


for i in {1..3}
do
for numThr in  5 10
do
for p in 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0
do
./mvccBanking-$numThr-$p.out	
done
done
done

cat header out > 7b-omvcc.csv

rm -f header out

for i in {1..3}
do
for numThr in  5 10
do
for p in 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0
do
./mv3cBanking-$numThr-$p.out	
done
done
done

cat header out > 7b-mv3c.csv
rm -f header out


for i in {1..3}
do
for numThr in  5 10
do
for p in 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0
do
./mv3cBankingCrit-$numThr-$p.out	
done
done
done

cat header out > 7b-mv3cCrit.csv
rm -f header out
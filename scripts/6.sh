#!/bin/bash
CC=g++
rm -rf *Trading*.out
rm -f 6*.csv
rm -f header out
echo "Trading Combined test Transactions=$num"
for p in 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.1 2.2 2.3 2.4
do
for numThr in {1..10}
do
echo -n "$numThr $p " 
done
done | xargs  -n 2 -P10 ./compileTrading.sh

for i in {1..3}
do
for p in 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.1 2.2 2.3 2.4
do
for numThr in {1..10}
do
./mvccTrading-$numThr-$p.out	
done
done
done

cat header out > 6-omvcc.csv

rm -f header out

for i in {1..3}
do
for p in 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.1 2.2 2.3 2.4
do
for numThr in {1..10}
do
./mv3cTrading-$numThr-$p.out	
done
done
done

cat header out > 6-mv3c.csv
rm -f header out




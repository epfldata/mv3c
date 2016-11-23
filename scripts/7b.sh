#!/bin/bash

rm -rf *Banking*.out
rm -f 7b*.csv 7b.avg 7b.pdf
rm -f header out
echo "Banking  conflict test"
numThr=10
for p in 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 ./compileBanking.sh


for i in {1..5}
do
for p in 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0
do
sudo ./mvccBanking-$numThr-$p-no.out
sudo ./mv3cBanking-$numThr-$p-no.out			
sudo ./mvccBanking-$numThr-$p-je.out
sudo ./mv3cBanking-$numThr-$p-je.out			
done
done

cat header out > 7b.csv

rm -f header out

#python 7b.py
#gnuplot 7b.plot
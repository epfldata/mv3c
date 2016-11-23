#!/bin/bash

rm -rf *Trading*.out
rm -f 6a*.csv 6a.avg 6a.pdf
rm -f header out
echo "Trading thread test"
p=1.4
for numThr in {1..11}
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 ./compileTrading.sh

for i in {1..5}
do
for numThr in {1..11}
do
sudo ./mvccTrading-$numThr-$p-no.out	
sudo ./mv3cTrading-$numThr-$p-no.out	
sudo ./mvccTrading-$numThr-$p-je.out	
sudo ./mv3cTrading-$numThr-$p-je.out	
done
done

cat header out > 6a.csv

rm -f header out

#python 6a.py
#gnuplot 6a.plot


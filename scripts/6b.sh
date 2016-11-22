#!/bin/bash

rm -rf *Trading*.out
rm -f 6b*.csv 6b.avg 6b.pdf
rm -f header out
echo "Trading conflict test"

numThr=10
for p in 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.1 2.2 2.3 2.4
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 ./compileTrading.sh

for i in {1..5}
do
for p in 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.1 2.2 2.3 2.4
do
sudo ./mvccTrading-$numThr-$p.out	
sudo ./mv3cTrading-$numThr-$p.out	
done
done

cat header out > 6b.csv

rm -f header out

python 6b.py
gnuplot 6b.plot


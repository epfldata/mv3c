#!/bin/bash

rm -rf *Trading*.out
rm -f 6a*.csv 6a.avg 6a.pdf
rm -f header out
echo "Trading thread test"
p=1.4

#Compile
for numThr in {1..11}
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 ./compileTrading.sh


#MVCC
for numThr in {1..11}
do
for i in {1..10}
do
sudo ./mvccTrading-$numThr-$p.out		
done
done


#MV3C
for numThr in {1..11}
do
for i in {1..10}
do
sudo ./mv3cTrading-$numThr-$p.out
done
done

cat header out > 6a.csv
rm -f header out

#python 6a.py
#gnuplot 6a.plot


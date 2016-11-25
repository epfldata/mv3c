#!/bin/bash

rm -rf *Banking*.out
rm -f 7a*.csv 7a.avg 7a.pdf
rm -f header out
echo "Banking  thread test"
p=1.0

#Compile
for numThr in  {1..11}
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 ./compileBanking.sh

#MVCC
for numThr in  {1..11}
do
for i in {1..10}
do
sudo ./mvccBanking-$numThr-$p.out
done
done

#MV3C
for numThr in  {1..11}
do
for i in {1..10}
do
sudo ./mv3cBanking-$numThr-$p.out
done
done

 
cat header out > 7a.csv
rm -f header out

#python 7a.py
#gnuplot 7a.plot
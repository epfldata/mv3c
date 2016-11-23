#!/bin/bash

rm -rf *Banking*.out
rm -f 7a*.csv 7a.avg 7a.pdf
rm -f header out
echo "Banking  thread test"
p=1.0
for numThr in  {1..11}
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 ./compileBanking.sh

for i in {1..5}
do
for numThr in  {1..11}
do
sudo ./mvccBanking-$numThr-$p-no.out
sudo ./mv3cBanking-$numThr-$p-no.out
sudo ./mvccBanking-$numThr-$p-je.out
sudo ./mv3cBanking-$numThr-$p-je.out
#sudo ./mv3cBankingCrit-$numThr-$p-$feeAccs.out	

done
done 
cat header out > 7a.csv
rm -f header out

#python 7a.py
#gnuplot 7a.plot
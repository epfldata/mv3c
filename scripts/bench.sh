#!/bin/bash

rm -rf *bench*.out
rm -f bench*.csv
rm -f header out
echo "Bench"
for p in  1 10 100
do
for numThr in {1..5}
do
echo -n "$numThr $p " 
done
done | xargs -n 2 -P10 ./compileBench.sh


#for p in 1 #10 100 
#do
#for numThr in {1..6}
#do
#for i in {1..3}
#do
#./bench-csi-$numThr-$p.out	
#done
#done
#done

#for p in 1 #10 100
#do
#for numThr in {1..6}
#do
#for i in {1..3}
#do
#./bench-mmsi-$numThr-$p.out	
#done
#done
#done


for p in 1 10 100 
do
for numThr in {1..5}
do
for i in {1..5}
do
sudo ./bench-ccsi-$numThr-$p.out	
done
done
done

#
#for p in 1 #10 100
#do
#for numThr in {1..6}
#do
#for i in {1..3}
#do
#./bench-mpi-$numThr-$p.out	
#done
#done
#done


for p in 1 10 100 
do
for numThr in {1..5}
do
for i in {1..5}
do
sudo ./bench-cpi-$numThr-$p.out	
done
done
done


cat header out > bench.csv

rm -f header out




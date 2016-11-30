#!/bin/bash
rm -rf *TPCC*.out
rm -f 8a*.csv 8a*.avg 8a*.pdf
rm -f header out
echo "TPCC  thread test "

#Compile
for p in 1 2
do
for numThr in  {1..11}
do
echo -n "$numThr $p " 
done
done | xargs  -n 2 -P10 ./compileTPCC.sh

rm ~/TStore/CavCommands*
cp ~/Full/CavCommands* ~/TStore/

#MVCC 1
for p in 1
do
for numThr in  {1..11}
do
for i in {1..10}
do
sudo ./mvccTPCC-$numThr-$p.out						
done
done
done


#MV3C 1
for p in 1
do
for numThr in  {1..11}
do
for i in {1..10}
do
sudo ./mv3cTPCC-$numThr-$p.out						
done
done
done

cat header out > 8a1.csv
rm -f header out



#MVCC 2
for p in 2
do
for numThr in  {1..11}
do
for i in {1..10}
do
sudo ./mvccTPCC-$numThr-$p.out						
done
done
done


#MV3C 2
for p in 2
do
for numThr in  {1..11}
do
for i in {1..10}
do
sudo ./mv3cTPCC-$numThr-$p.out						
done
done
done

cat header out > 8a2.csv
rm -f header out

python 8a-thr.py
gnuplot 8a-thr.plot
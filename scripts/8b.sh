#!/bin/bash
rm -rf *TPCC*.out
rm -f 8b*.csv 8b.avg 8b.pdf
rm -f header out
echo "TPCC  conflict test "

#Compile
for numThr in 10  
do
for p in 1 2 3 4 6 8 10 12 14 16   
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 ./compileTPCC.sh
done

rm ~/TStore/CavCommands*
cp ~/Full/CavCommands* ~/TStore/

#MVCC
for numThr in 10
do
for p in 1 2 3 4 6 8 10 12 14 16
do
for i in {1..10} 
do
sudo ./mvccTPCC-$numThr-$p.out							
done
done
done


#MV3C
for numThr in 10
do
for p in 1 2 3 4 6 8 10 12 14 16
do
for i in {1..10} 
do
sudo ./mv3cTPCC-$numThr-$p.out						
done
done
done

cat header out > 8b.csv
rm -f header out


python 8b-thr.py
gnuplot 8b-thr.plot



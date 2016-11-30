#!/bin/bash
rm -rf *TPCC2*.out
rm -f 10b*.csv
rm -f header out
echo "TPCC_2  conflict test "
for numThr in 10  
do
for p in 1 2 3 4 6 8 10   
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 ./compileTPCC2.sh
done

for i in 1 2 3 
do
for numThr in 10 
do
for p in 1 2 3 4 6 8 10
do
./mvccTPCC2-$numThr-$p.out	
./mv3cTPCC2-$numThr-$p.out		
done
done
done

cat header out > 10b.csv
rm -f header out

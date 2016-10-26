#!/bin/bash
rm -rf *TPCC*.out
rm -f 9a*.csv
rm -f header out
echo "TPCC  thread test "

for p in 1 2 3
do
for numThr in {1..12}
do
echo -n "$numThr $p " 
done
done | xargs  -n 2 -P10 ./compileTPCC.sh

rm ~/TStore/CavCommands*
cp ~/Full/CavCommands* ~/TStore/
for i in {1..3} 
do
for p in 1 2 3
do
for numThr in  {1..12}
do
./mvccTPCC-$numThr-$p.out	
./mv3cTPCC-$numThr-$p.out		
done
done
done

cat header out > 9a-full.csv
rm -f header out



rm ~/TStore/CavCommands*
cp ~/NOPY/CavCommands* ~/TStore/
for i in {1..3} 
do
for p in 1 2 3
do
for numThr in  {1..12}
do
./mvccTPCC-$numThr-$p.out	
./mv3cTPCC-$numThr-$p.out		
done
done
done

cat header out > 9a-nopy.csv
rm -f header out




rm ~/TStore/CavCommands*
cp ~/PY/CavCommands* ~/TStore/
for i in {1..3} 
do
for p in 1 2 3
do
for numThr in  {1..12}
do
./mvccTPCC-$numThr-$p.out	
./mv3cTPCC-$numThr-$p.out		
done
done
done

cat header out > 9a-py.csv
rm -f header out
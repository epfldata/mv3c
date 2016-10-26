#!/bin/bash
rm -rf *TPCC*.out
rm -f 9b*.csv
rm -f header out
echo "TPCC  conflict test "
for numThr in 10 9 
do
for p in 1 2 3 4 5 6 7 8 10 12 14 16 
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 ./compileTPCC.sh
done

rm ~/TStore/CavCommands*
cp ~/Full/CavCommands* ~/TStore/
for i in 1
do
for numThr in 10 9
do
for p in 1 2 3 4 5 6 7 8 10 12 14 16 
do
./mvccTPCC-$numThr-$p.out	
./mv3cTPCC-$numThr-$p.out		
done
done
done

cat header out > 9b-full.csv
rm -f header out



#rm ~/TStore/CavCommands*
#cp ~/NOPY/CavCommands* ~/TStore/
#for i in {1..3}
#do
#for p in 1 2 3 4 5 6 7 8 10 12 14 16 
#do
#./mvccTPCC-$numThr-$p.out	
#./mv3cTPCC-$numThr-$p.out		
#done
#done
#
#cat header out > 9b-nopy.csv
#rm -f header out
#
#
#
#rm ~/TStore/CavCommands*
#cp ~/PY/CavCommands* ~/TStore/
#for i in {1..3}
#do
#for p in 1 2 3 4 5 6 7 8 10 12 14 16 
#do
#./mvccTPCC-$numThr-$p.out	
#./mv3cTPCC-$numThr-$p.out		
#done
#done
#
#cat header out > 9b-py.csv
#rm -f header out

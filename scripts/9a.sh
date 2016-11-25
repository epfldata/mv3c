#!/bin/bash
rm -rf *TPCC*.out
rm -f 9a*.csv 9a*.avg 9a*.pdf
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

cat header out > 9a1.csv
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

cat header out > 9a2.csv
rm -f header out
#python 9a.py
#gnuplot 9a.plot

#rm ~/TStore/CavCommands*
#cp ~/NOPY/CavCommands* ~/TStore/
#for i in {1..3} 
#do
#for p in 1 2 3
#do
#for numThr in  {1..12}
#do
#./mvccTPCC-$numThr-$p.out	
#./mv3cTPCC-$numThr-$p.out		
#done
#done
#done
#
#cat header out > 9a-nopy.csv
#rm -f header out
#



#rm ~/TStore/CavCommands*
#cp ~/PY2/CavCommands* ~/TStore/
#for i in 1
#do
#for p in 1 2
#do
#for numThr in  {1..12}
#do
#./mvccTPCC-$numThr-$p.out	
#./mv3cTPCC-$numThr-$p.out		
#done
#done
#done

#cat header out > 9a-py2.csv
#rm -f header out
#!/bin/bash
ROOT="$(dirname "$0")/.."
CC=g++

rm -rf *Banking*.out
rm -f 7a*.csv
rm -f header out
num=5000000
p=1.0
profile= #-DDTIMER 
ww=-DALLOW_WW
critrex="-DCRITICAL_COMPENSATE=true"
store= #-DSTORE_ENABLE
attrib= #-DATTRIB_LEVEL
echo "Banking  thread test Transactions=$num"
for numThr in {1..2}
do
$CC -DOMVCC=true -DBANKING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mvccBanking$numThr.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash
$CC $ww $critrex -DBANKING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $atrrib -DCONFLICT_FRACTION=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mv3cBanking$numThr.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash 
done

for numThr in {1..2}
do
./mvccBanking$numThr.out	
done

cat header out > 7a-omvcc.csv

rm -f header out
for numThr in {1..2}
do
./mv3cBanking$numThr.out	
done
cat header out > 7a-mv3c.csv
rm -f header out

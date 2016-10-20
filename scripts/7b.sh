#!/bin/bash
ROOT="$(dirname "$0")/.."
CC=g++

rm -rf *Banking*.out
rm -f 7b*.csv
rm -f header out
num=5000000
profile= #-DDTIMER 
ww=-DALLOW_WW
critrex= #-DCRITICAL_COMPENSATE=true
store= #-DSTORE_ENABLE
attrib= #-DATTRIB_LEVEL

echo "Banking  conflict test Transactions=$num"
numThr=5
for p in 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0
do
$CC -DOMVCC=true -DBANKING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mvccBanking$p.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash
$CC $ww $critrex -DBANKING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $atrrib -DCONFLICT_FRACTION=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mv3cBanking$p.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash 
done

for p in 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0
do
./mvccBanking$p.out	
done

cat header out > 7b-omvcc.csv

rm -f header out
for p in 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0
do
./mv3cBanking$p.out	
done
cat header out > 7b-mv3c.csv
rm -f header out





numThr=10
for p in 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0
do
$CC -DOMVCC=true -DBANKING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mvccBanking$p.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash
$CC $ww $critrex -DBANKING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $atrrib -DCONFLICT_FRACTION=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mv3cBanking$p.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash 
done

for p in 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0
do
./mvccBanking$p.out	
done

cat out >> 7b-omvcc.csv

rm -f header out
for p in 0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0
do
./mv3cBanking$p.out	
done
cat out >> 7b-mv3c.csv
rm -f header out


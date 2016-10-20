#!/bin/bash
ROOT="$(dirname "$0")/.."
CC=g++
CRYPTO=cryptopp	#cryptopp or crypto++
rm -rf *TPCC*.out
rm -f 9b*.csv
rm -f header out
num=1000000

profile= #-DDTIMER 
ww=-DALLOW_WW
critrex= #-DCRITICAL_COMPENSATE=true
store= #-DSTORE_ENABLE
attrib=-DATTRIB_LEVEL
echo "TPCC ware test Transactions=$num"
numThr=5
for w in 1 2 4 6 8 10 12 16 20
do
$CC -DOMVCC=true -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=w -DNDEBUG -DNUMPROG=$num  -Wno-attributes   -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mvccTPCCw$w.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
$CC $ww $critrex -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $atrrib -DNUMWARE=w -DNDEBUG -DNUMPROG=$num  -Wno-attributes   -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mv3cTPCCw$w.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
done

for w in 1 2 4 6 8 10 12 16 20
do
./mvccTPCCw$w.out	
done

cat header out > 9b-omvcc.csv

rm -f header out
for w in 1 2 4 6 8 10 12 16 20
do
./mv3cTPCCw$w.out	
done
cat header out > 9b-mv3c.csv
rm -f header out



numThr=10
for w in 1 2 4 6 8 10 12 16 20
do
$CC -DOMVCC=true -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=w -DNDEBUG -DNUMPROG=$num  -Wno-attributes   -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mvccTPCCw$w.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
$CC $ww $critrex -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $atrrib -DNUMWARE=w -DNDEBUG -DNUMPROG=$num  -Wno-attributes   -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mv3cTPCCw$w.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
done

for w in 1 2 4 6 8 10 12 16 20
do
./mvccTPCCw$w.out	
done

cat  out >> 9b-omvcc.csv

rm -f header out
for w in 1 2 4 6 8 10 12 16 20
do
./mv3cTPCCw$w.out	
done
cat  out >> 9b-mv3c.csv
rm -f header out
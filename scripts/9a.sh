#!/bin/bash
ROOT="$(dirname "$0")/.."
CC=g++
CRYPTO=cryptopp	#cryptopp or crypto++
rm -rf *TPCC*.out
rm -f 9a*.csv
rm -f header out
num=1000000

profile= #-DDTIMER 
ww=-DALLOW_WW
critrex= #-DCRITICAL_COMPENSATE=true
store= #-DSTORE_ENABLE
attrib=-DATTRIB_LEVEL
echo "TPCC thread test Transactions=$num"
w=1
for numThr in {1..10}
do
$CC -DOMVCC=true -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=w -DNDEBUG -DNUMPROG=$num  -Wno-attributes   -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mvccTPCC$numThr.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
$CC $ww $critrex -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $atrrib -DNUMWARE=w -DNDEBUG -DNUMPROG=$num  -Wno-attributes   -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mv3cTPCC$numThr.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
done

for numThr in {1..10}
do
./mvccTPCC$numThr.out	
done

cat header out > 9a-omvcc.csv

rm -f header out
for numThr in {1..10}
do
./mv3cTPCC$numThr.out	
done
cat header out > 9a-mv3c.csv
rm -f header out




w=2
for numThr in {1..10}
do
$CC -DOMVCC=true -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=w -DNDEBUG -DNUMPROG=$num  -Wno-attributes   -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mvccTPCC$numThr.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
$CC $ww $critrex -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $atrrib -DNUMWARE=w -DNDEBUG -DNUMPROG=$num  -Wno-attributes   -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mv3cTPCC$numThr.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
done

for numThr in {1..10}
do
./mvccTPCC$numThr.out	
done

cat out >> 9a-omvcc.csv

rm -f header out
for numThr in {1..10}
do
./mv3cTPCC$numThr.out	
done
cat out >> 9a-mv3c.csv
rm -f header out




w=3
for numThr in {1..10}
do
$CC -DOMVCC=true -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=w -DNDEBUG -DNUMPROG=$num  -Wno-attributes   -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mvccTPCC$numThr.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
$CC $ww $critrex -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $atrrib -DNUMWARE=w -DNDEBUG -DNUMPROG=$num  -Wno-attributes   -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mv3cTPCC$numThr.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
done

for numThr in {1..10}
do
./mvccTPCC$numThr.out	
done

cat out >> 9a-omvcc.csv

rm -f header out
for numThr in {1..10}
do
./mv3cTPCC$numThr.out	
done
cat out >> 9a-mv3c.csv
rm -f header out
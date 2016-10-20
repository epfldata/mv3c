#!/bin/bash
ROOT="$(dirname "$0")/.."
CC=g++
CRYPTO=cryptopp	#cryptopp or crypto++
rm -rf *Trading*.out
rm -f 6a*.csv
rm -f header out
num=5000000
p=2
profile= #-DDTIMER 
ww=-DALLOW_WW
critrex= #-DCRITICAL_COMPENSATE=true
store= #-DSTORE_ENABLE
attrib= #-DATTRIB_LEVEL
echo "Trading thread test Transactions=$num"
for numThr in {1..10}
do
$CC -DOMVCC=true -DTRADING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DPOWER=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I"/usr/include/$CRYPTO" -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mvccTrading$numThr.out"  $ROOT/mainTrading.cpp  -pthread -lcityhash "-l$CRYPTO"
$CC $ww $critrex -DTRADING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $atrrib -DPOWER=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I"/usr/include/$CRYPTO" -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mv3cTrading$numThr.out"  $ROOT/mainTrading.cpp  -pthread -lcityhash "-l$CRYPTO"
done

for numThr in {1..10}
do
./mvccTrading$numThr.out	
done

cat header out > 6a-omvcc.csv

rm -f header out
for numThr in {1..10}
do
./mv3cTrading$numThr.out	
done
cat header out > 6a-mv3c.csv
rm -f header out




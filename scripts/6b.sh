#!/bin/bash
ROOT="$(dirname "$0")/.."
CC=g++
CRYPTO=cryptopp	#cryptopp or crypto++
rm -rf *Trading*.out
rm -f 6b*.csv
rm -f header out
num=5000000

profile= #-DDTIMER 
ww=-DALLOW_WW
critrex= #-DCRITICAL_COMPENSATE=true
store= #-:
attrib= #-DATTRIB_LEVEL
echo "Trading percentage conflict test Transactions=$num"
numThr=5 
for p in 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0
do
$CC -DOMVCC=true -DTRADING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DPOWER=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I"/usr/include/$CRYPTO" -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mvccTrading$p.out"  $ROOT/mainTrading.cpp  -pthread -lcityhash "-l$CRYPTO"
$CC $ww $critrex -DTRADING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $atrrib -DPOWER=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I"/usr/include/$CRYPTO" -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mv3cTrading$p.out"  $ROOT/mainTrading.cpp  -pthread -lcityhash "-l$CRYPTO"
done

for p in 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0
do
./mvccTrading$p.out	
done

cat header out > 6b-omvcc.csv

rm -rf header out
for p in 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0
do
./mv3cTrading$p.out	
done
cat header out > 6b-mv3c.csv
rm -rf header out




numThr=10 
for p in 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0
do
$CC -DOMVCC=true -DTRADING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DPOWER=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I"/usr/include/$CRYPTO" -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mvccTrading$p.out"  $ROOT/mainTrading.cpp  -pthread -lcityhash "-l$CRYPTO"
$CC $ww $critrex -DTRADING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $atrrib -DPOWER=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I"/usr/include/$CRYPTO" -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mv3cTrading$p.out"  $ROOT/mainTrading.cpp  -pthread -lcityhash "-l$CRYPTO"
done

for p in 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0
do
./mvccTrading$p.out	
done

cat out >> 6b-omvcc.csv

rm -rf header out
for p in 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0
do
./mv3cTrading$p.out	
done
cat out >> 6b-mv3c.csv
rm -rf header out


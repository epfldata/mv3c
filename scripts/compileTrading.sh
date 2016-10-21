#!/bin/bash
ROOT="$(dirname "$0")/.."
CC=g++
CRYPTO=cryptopp	#cryptopp or crypto++

num=5000000
profile= #-DDTIMER 
ww=-DALLOW_WW
critrex= #-DCRITICAL_COMPENSATE=true
store= #-DSTORE_ENABLE
attrib= #-DATTRIB_LEVEL
numThr=$1
p=$2

$CC -DOMVCC=true -DTRADING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DPOWER=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I"/usr/include/$CRYPTO" -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mvccTrading-$numThr-$p.out"  $ROOT/mainTrading.cpp  -pthread -lcityhash "-l$CRYPTO"
$CC $ww $critrex -DTRADING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $atrrib -DPOWER=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I"/usr/include/$CRYPTO" -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mv3cTrading-$numThr-$p.out"  $ROOT/mainTrading.cpp  -pthread -lcityhash "-l$CRYPTO"

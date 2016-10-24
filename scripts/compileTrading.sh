#!/bin/bash
ROOT="$(dirname "$0")/.."
CC=g++
CRYPTO=cryptopp	#cryptopp or crypto++

num=5000000
profile= #-DDTIMER 
ww=-DALLOW_WW
critrex=false
store= #-DSTORE_ENABLE
attrib= #-DATTRIB_LEVEL
cuckoo=false
numThr=$1
p=$2

$CC -DOMVCC=true                       -DTRADING_TEST -DCUCKOO=$cuckoo -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DPOWER=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I"/usr/include/$CRYPTO" -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mvccTrading-$numThr-$p.out"  $ROOT/mainTrading.cpp  -pthread -lcityhash "-l$CRYPTO"
$CC $ww -DCRITICAL_COMPENSATE=$critrex -DTRADING_TEST -DCUCKOO=$cuckoo -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DPOWER=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I"/usr/include/$CRYPTO" -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mv3cTrading-$numThr-$p.out"  $ROOT/mainTrading.cpp  -pthread -lcityhash "-l$CRYPTO"

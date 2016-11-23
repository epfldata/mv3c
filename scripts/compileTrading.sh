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
cuckoo=true
numThr=$1
p=$2

$CC -DOMVCC=true                       -DTRADING_TEST -DCUCKOO=$cuckoo -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DPOWER=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I"/usr/include/$CRYPTO" -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -DMALLOCTYPE=\"normal\"  -o "mvccTrading-$numThr-$p-no.out"  $ROOT/mainTrading.cpp  -pthread -lcityhash "-l$CRYPTO"
$CC $ww -DCRITICAL_COMPENSATE=$critrex -DTRADING_TEST -DCUCKOO=$cuckoo -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DPOWER=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I"/usr/include/$CRYPTO" -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -DMALLOCTYPE=\"normal\"  -o "mv3cTrading-$numThr-$p-no.out"  $ROOT/mainTrading.cpp  -pthread -lcityhash "-l$CRYPTO"
$CC -DOMVCC=true                       -DTRADING_TEST -DCUCKOO=$cuckoo -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DPOWER=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I"/usr/include/$CRYPTO" -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -DMALLOCTYPE=\"jemalloc\"  -o "mvccTrading-$numThr-$p-je.out"  $ROOT/mainTrading.cpp  -pthread -lcityhash "-l$CRYPTO" -ljemalloc
$CC $ww -DCRITICAL_COMPENSATE=$critrex -DTRADING_TEST -DCUCKOO=$cuckoo -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DPOWER=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I"/usr/include/$CRYPTO" -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -DMALLOCTYPE=\"jemalloc\"  -o "mv3cTrading-$numThr-$p-je.out"  $ROOT/mainTrading.cpp  -pthread -lcityhash "-l$CRYPTO" -ljemalloc

#!/bin/bash
ROOT="$(dirname "$0")/.."
CC=g++

num=5000000
profile=-DDTIMER 
ww=-DALLOW_WW
critrex=true
store= #-DSTORE_ENABLE
attrib= #-DATTRIB_LEVEL
cuckoo=false
p=$2
numThr=$1
$CC -DOMVCC=true                    -DBANKING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mvccBanking-$numThr-$p.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash
$CC $ww -DCRITICAL_COMPENSATE=true  -DBANKING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $atrrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mv3cBankingCrit-$numThr-$p.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash 
$CC $ww -DCRITICAL_COMPENSATE=false -DBANKING_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $atrrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  -o "mv3cBanking-$numThr-$p.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash 

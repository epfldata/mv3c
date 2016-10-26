#!/bin/bash
ROOT="$(dirname "$0")/.."
CC=g++

num=1000000
profile= #-DDTIMER 
ww=-DALLOW_WW
critrex=false
store= #-DSTORE_ENABLE
attrib=-DATTRIB_LEVEL
cuckoo=true
si=-DCUCKOO_SI
p=$2
numThr=$1
$CC -DOMVCC=true                        -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  $si  -o "mvccTPCC-$numThr-$p.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash
$CC $ww -DCRITICAL_COMPENSATE=$critrex  -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc  -std=c++11  $si  -o "mv3cTPCC-$numThr-$p.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 

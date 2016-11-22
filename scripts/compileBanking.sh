#!/bin/bash
ROOT="$(dirname "$0")/.."
CC=g++

num=5000000
profile= #-DDTIMER 
ww=-DALLOW_WW
critrex=true
store= #-DSTORE_ENABLE
attrib= #-DATTRIB_LEVEL
cuckoo=false
execprofile= #-DEXEC_PROFILE
p=$2
numThr=$1
feeAccs=1
$CC -DOMVCC=true                    $execprofile -DBANKING_TEST -DNUMTHREADS=$numThr -DFEEACCOUNTS=$feeAccs -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -o "mvccBanking-$numThr-$p.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash -ljemalloc
$CC $ww -DCRITICAL_COMPENSATE=$critrex  $execprofile -DBANKING_TEST -DNUMTHREADS=$numThr -DFEEACCOUNTS=$feeAccs -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -o "mv3cBanking-$numThr-$p.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash -ljemalloc
#$CC $ww -DCRITICAL_COMPENSATE=false $execprofile -DBANKING_TEST -DNUMTHREADS=$numThr -DFEEACCOUNTS=$feeAccs -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -o "mv3cBankingCrit-$numThr-$p-$feeAccs.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash -ljemalloc

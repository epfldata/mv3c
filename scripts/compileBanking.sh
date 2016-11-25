#!/bin/bash
ROOT="$(dirname "$0")/.."
CC=g++

num=5000000
profile= #-DDTIMER 
ww=-DALLOW_WW
store= #-DSTORE_ENABLE
attrib= #-DATTRIB_LEVEL
cuckoo=false
execprofile= #-DEXEC_PROFILE
p=$2
numThr=$1
feeAccs=1
$CC -DOMVCC=true $execprofile -DBANKING_TEST -DNUMTHREADS=$numThr -DFEEACCOUNTS=$feeAccs -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -DMALLOCTYPE=\"normal\" -o "mvccBanking-$numThr-$p.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash 
$CC $ww          $execprofile -DBANKING_TEST -DNUMTHREADS=$numThr -DFEEACCOUNTS=$feeAccs -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -DMALLOCTYPE=\"normal\"  -o "mv3cBanking-$numThr-$p.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash 
#$CC $ww          $execprofile -DBANKING_TEST -DNUMTHREADS=$numThr -DFEEACCOUNTS=$feeAccs -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -DMALLOCTYPE=\"normal\" -DCRITICAL_COMPENSATE_THRESHOLD=1 -o "mv3cBanking-$numThr-$p-1.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash 
#$CC $ww          $execprofile -DBANKING_TEST -DNUMTHREADS=$numThr -DFEEACCOUNTS=$feeAccs -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -DMALLOCTYPE=\"normal\" -DCRITICAL_COMPENSATE_THRESHOLD=2 -o "mv3cBanking-$numThr-$p-2.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash 
#$CC $ww          $execprofile -DBANKING_TEST -DNUMTHREADS=$numThr -DFEEACCOUNTS=$feeAccs -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -DMALLOCTYPE=\"normal\" -DCRITICAL_COMPENSATE_THRESHOLD=3 -o "mv3cBanking-$numThr-$p-3.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash 
#$CC $ww          $execprofile -DBANKING_TEST -DNUMTHREADS=$numThr -DFEEACCOUNTS=$feeAccs -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -DMALLOCTYPE=\"normal\" -DCRITICAL_COMPENSATE_THRESHOLD=5 -o "mv3cBanking-$numThr-$p-5.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash 
#$CC $ww          $execprofile -DBANKING_TEST -DNUMTHREADS=$numThr -DFEEACCOUNTS=$feeAccs -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -DMALLOCTYPE=\"normal\" -DCRITICAL_COMPENSATE_THRESHOLD=8 -o "mv3cBanking-$numThr-$p-8.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash 
#$CC $ww          $execprofile -DBANKING_TEST -DNUMTHREADS=$numThr -DFEEACCOUNTS=$feeAccs -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -DMALLOCTYPE=\"normal\" -DCRITICAL_COMPENSATE_THRESHOLD=20 -o "mv3cBanking-$numThr-$p-20.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash 
#$CC $ww          $execprofile -DBANKING_TEST -DNUMTHREADS=$numThr -DFEEACCOUNTS=$feeAccs -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -DMALLOCTYPE=\"normal\" -DCRITICAL_COMPENSATE_THRESHOLD=10 -o "mv3cBanking-$numThr-$p-10.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash 
#$CC $ww          $execprofile -DBANKING_TEST -DNUMTHREADS=$numThr -DFEEACCOUNTS=$feeAccs -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -DMALLOCTYPE=\"normal\" -DCRITICAL_COMPENSATE_THRESHOLD=100 -o "mv3cBanking-$numThr-$p-100.out"  $ROOT/mainBanking.cpp  -pthread -lcityhash 

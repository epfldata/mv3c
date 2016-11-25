#!/bin/bash
ROOT="$(dirname "$0")/.."
CC=g++

num=1000000
profile= #-DDTIMER 
ww=-DALLOW_WW
store= #-DSTORE_ENABLE
attrib=-DATTRIB_LEVEL
cuckoo=true
#cww=false
si=-DCUCKOO_SI
p=$2
numThr=$1
$CC -DOMVCC=true -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  $si -DCWW=false -DMALLOCTYPE=\"normal\" -o "mvccTPCC-$numThr-$p.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
$CC $ww          -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  $si -DCWW=false -DMALLOCTYPE=\"normal\" -o "mv3cTPCC-$numThr-$p.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
#$CC $ww          -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  $si -DCWW=false -DMALLOCTYPE=\"normal\" -DCRITICAL_COMPENSATE_THRESHOLD=1 -o "mv3cTPCC-$numThr-$p-1.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
#$CC $ww          -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  $si -DCWW=false -DMALLOCTYPE=\"normal\" -DCRITICAL_COMPENSATE_THRESHOLD=3 -o "mv3cTPCC-$numThr-$p-3.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
#$CC $ww          -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  $si -DCWW=false -DMALLOCTYPE=\"normal\" -DCRITICAL_COMPENSATE_THRESHOLD=5 -o "mv3cTPCC-$numThr-$p-5.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
#$CC $ww          -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  $si -DCWW=false -DMALLOCTYPE=\"normal\" -DCRITICAL_COMPENSATE_THRESHOLD=8 -o "mv3cTPCC-$numThr-$p-8.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
#$CC $ww          -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  $si -DCWW=false -DMALLOCTYPE=\"normal\" -DCRITICAL_COMPENSATE_THRESHOLD=10 -o "mv3cTPCC-$numThr-$p-10.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
#$CC $ww          -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  $si -DCWW=false -DMALLOCTYPE=\"normal\" -DCRITICAL_COMPENSATE_THRESHOLD=50 -o "mv3cTPCC-$numThr-$p-50.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
#$CC $ww          -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  $si -DCWW=false -DMALLOCTYPE=\"normal\" -DCRITICAL_COMPENSATE_THRESHOLD=100 -o "mv3cTPCC-$numThr-$p-100.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 

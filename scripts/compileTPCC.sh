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
#cww=false
si=-DCUCKOO_SI
p=$2
numThr=$1
$CC -DOMVCC=true                        -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  $si -DCWW=false -DMALLOCTYPE=\"normal\" -o "mvccTPCC-$numThr-$p-no.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
$CC $ww -DCRITICAL_COMPENSATE=$critrex  -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  $si -DCWW=false -DMALLOCTYPE=\"normal\" -o "mv3cTPCC-$numThr-$p-no.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash 
$CC -DOMVCC=true                        -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  $si -DCWW=false -DMALLOCTYPE=\"jemalloc\" -o "mvccTPCC-$numThr-$p-je.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash -ljemalloc
$CC $ww -DCRITICAL_COMPENSATE=$critrex  -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  $si -DCWW=false -DMALLOCTYPE=\"jemalloc\" -o "mv3cTPCC-$numThr-$p-je.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash -ljemalloc
#$CC -DOMVCC=true                        -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  $si -DCWW=false -o "mvccTPCC-$numThr-tc-$p.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash -ltcmalloc_minimal
#$CC $ww -DCRITICAL_COMPENSATE=$critrex  -DTPCC_TEST -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  $si -DCWW=false -o "mv3cTPCC-$numThr-tc-$p.out"  $ROOT/mainTPCC.cpp  -pthread -lcityhash -ltcmalloc_minimal

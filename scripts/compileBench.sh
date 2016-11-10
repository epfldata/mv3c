#!/bin/bash
ROOT="$(dirname "$0")/.."
CC=g++

numThr=$1
p=$2
#$CC -DCSI -DNDEBUG -DBENCH -DSCALE=$p -DNUMTHREADS=$numThr -m64 -O3  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -o "bench-csi-$numThr-$p.out"  $ROOT/mainBench.cpp  -pthread -lcityhash
#$CC -DMMSI -DNDEBUG -DBENCH -DSCALE=$p -DNUMTHREADS=$numThr -m64 -O3  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -o "bench-mmsi-$numThr-$p.out"  $ROOT/mainBench.cpp  -pthread -lcityhash
$CC -DCCSI -DNDEBUG -DBENCH -DSCALE=$p -DNUMTHREADS=$numThr -m64 -O3  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -o "bench-ccsi-$numThr-$p.out"  $ROOT/mainBench.cpp  -pthread -lcityhash
$CC -DCPI -DNDEBUG -DBENCH -DSCALE=$p -DNUMTHREADS=$numThr -m64 -O3  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -o "bench-cpi-$numThr-$p.out"  $ROOT/mainBench.cpp  -pthread -lcityhash
#$CC -DMPI -DNDEBUG -DBENCH -DSCALE=$p -DNUMTHREADS=$numThr -m64 -O3  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -o "bench-mpi-$numThr-$p.out"  $ROOT/mainBench.cpp  -pthread -lcityhash

#$CC -DCCSI -DPERF_RECORD -DNDEBUG -DBENCH -DSCALE=$p -DNUMTHREADS=$numThr -m64 -O3  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -o "bench-ccsi-rec-$numThr-$p.out"  $ROOT/mainBench.cpp  -pthread -lcityhash
#$CC -DCCSI -DPERF_STAT -DNDEBUG -DBENCH -DSCALE=$p -DNUMTHREADS=$numThr -m64 -O3  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -o "bench-ccsi-stat-$numThr-$p.out"  $ROOT/mainBench.cpp  -pthread -lcityhash

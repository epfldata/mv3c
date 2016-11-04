#!/bin/bash
ROOT="$(dirname "$0")/.."
CC=g++

numThr=$1
p=$2
$CC -DCSI -DBENCH -DSCALE=$p -DNUMTHREADS=$numThr -m64 -O3  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -o "bench-csi-$numThr-$p.out"  $ROOT/mainBench.cpp  -pthread -lcityhash
$CC -DMMSI -DBENCH -DSCALE=$p -DNUMTHREADS=$numThr -m64 -O3  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -o "bench-mmsi-$numThr-$p.out"  $ROOT/mainBench.cpp  -pthread -lcityhash
$CC -DCCSI -DBENCH -DSCALE=$p -DNUMTHREADS=$numThr -m64 -O3  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -o "bench-ccsi-$numThr-$p.out"  $ROOT/mainBench.cpp  -pthread -lcityhash
$CC -DCPI -DBENCH -DSCALE=$p -DNUMTHREADS=$numThr -m64 -O3  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -o "bench-cpi-$numThr-$p.out"  $ROOT/mainBench.cpp  -pthread -lcityhash
$CC -DMPI -DBENCH -DSCALE=$p -DNUMTHREADS=$numThr -m64 -O3  -Wno-attributes  -I $ROOT/util -I $ROOT/mv3c -I $ROOT/trading -I $ROOT/banking -I $ROOT/tpcc -I $ROOT/tpcc2 -std=c++11  -o "bench-mpi-$numThr-$p.out"  $ROOT/mainBench.cpp  -pthread -lcityhash

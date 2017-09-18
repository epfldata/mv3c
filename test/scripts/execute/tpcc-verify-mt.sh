#!/bin/bash

ROOT="$(dirname "$0")/../../.."
EXE_DIR=`readlink -m "$(dirname "$0")/../output/executable/8"`
DATA_DIR=`readlink -m $ROOT`/
mkdir -p $EXE_DIR
CC=g++

num=${NUMXACTS:-1000000}
profile= #-DDTIMER 
ww=-DALLOW_WW
store= #-DSTORE_ENABLE
attrib=-DATTRIB_LEVEL
cuckoo=true
si=-DCUCKOO_SI

p=1
numThr=2

flags="-DTPCC_TEST -DVERIFY_TPCC_MT_ST $si -DCWW=false -DNUMTHREADS=$numThr -DTPCC_DATA_ROOT=\"$DATA_DIR\" -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes -I $ROOT/src -I $ROOT/test -std=c++11  -DMALLOCTYPE=\"normal\" "
src="$ROOT/test/tpcc/mainTPCC.cpp"
libs="-pthread -lcityhash" 

$CC -DOMVCC=true $flags -o "$EXE_DIR/mvccTPCC-$numThr-$p.out"  $src $libs
$CC $ww          $flags -o "$EXE_DIR/mv3cTPCC-$numThr-$p.out"  $src $libs


$EXE_DIR/mvccTPCC-$numThr-$p.out
$EXE_DIR/mv3cTPCC-$numThr-$p.out

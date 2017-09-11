#!/bin/bash
if [ "$#" -ne 3 ]; then
    echo "Illegal number of parameters"
fi

ROOT="$(dirname "$0")/../../.."
EXE_DIR=`readlink -m "$(dirname "$0")/../output/executable/$1"`
DATA_DIR="$ROOT/"
mkdir -p $EXE_DIR
CC=g++

num=${NUMXACTS:-1000000}
profile= #-DDTIMER 
ww=-DALLOW_WW
store= #-DSTORE_ENABLE
attrib=-DATTRIB_LEVEL
cuckoo=true
si=-DCUCKOO_SI

p=$3
numThr=$2

flags="-DTPCC_TEST $si -DCWW=false -DNUMTHREADS=$numThr -DTPCC_DATA_ROOT=\"$DATA_DIR\" -m64 -O3 $profile $store $attrib -DNUMWARE=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes -I $ROOT/src -I $ROOT/tests -std=c++11  -DMALLOCTYPE=\"normal\" "
src="$ROOT/tests/tpcc/mainTPCC.cpp"
libs="-pthread -lcityhash" 

$CC -DOMVCC=true $flags -o "$EXE_DIR/mvccTPCC-$numThr-$p.out"  $src $libs
$CC $ww          $flags -o "$EXE_DIR/mv3cTPCC-$numThr-$p.out"  $src $libs

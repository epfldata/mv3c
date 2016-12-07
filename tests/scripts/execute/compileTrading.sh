#!/bin/bash
if [ "$#" -ne 3 ]; then
    echo "Illegal number of parameters"
fi

ROOT="$(dirname "$0")/../../.."
EXE_DIR="$(dirname "$0")/../output/executable/$1"
mkdir -p $EXE_DIR
CC=g++
CRYPTO=cryptopp	#cryptopp or crypto++

num=5000000
profile= #-DDTIMER 
ww=-DALLOW_WW

store= #-DSTORE_ENABLE
attrib= #-DATTRIB_LEVEL
cuckoo=true

numThr=$2
p=$3


src="$ROOT/tests/trading/mainTrading.cpp"
flags="-DTRADING_TEST -DCUCKOO=$cuckoo -DNUMTHREADS=$numThr -m64 -O3 $profile $store $attrib -DPOWER=$p -DNDEBUG -DNUMPROG=$num  -Wno-attributes  -I /usr/include/$CRYPTO -I $ROOT/src -I $ROOT/tests -std=c++11  -DMALLOCTYPE=\"normal\" "
libs="-pthread -lcityhash -l$CRYPTO"

$CC -DOMVCC=true  $flags -o "$EXE_DIR/mvccTrading-$numThr-$p.out"  $src $libs 
$CC $ww           $flags -o "$EXE_DIR/mv3cTrading-$numThr-$p.out"  $src $libs

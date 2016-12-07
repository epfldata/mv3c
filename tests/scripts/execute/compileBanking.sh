#!/bin/bash
if [ "$#" -ne 3 ]; then
    echo "Illegal number of parameters"
fi
ROOT="$(dirname "$0")/../../.."
EXE_DIR="$(dirname "$0")/../output/executable/$1"
mkdir -p $EXE_DIR
CC=g++

num=5000000
profile= #-DDTIMER 
ww=-DALLOW_WW
store= #-DSTORE_ENABLE
attrib= #-DATTRIB_LEVEL
cuckoo=false
execprofile= #-DEXEC_PROFILE

feeAccs=1
p=$3
numThr=$2

src="$ROOT/tests/banking/mainBanking.cpp"
flags="-DBANKING_TEST -DNUMTHREADS=$numThr -DFEEACCOUNTS=$feeAccs -m64 -O3 $profile $store $attrib -DCONFLICT_FRACTION=$p -DCUCKOO=$cuckoo -DNDEBUG -DNUMPROG=$num  -Wno-attributes -I $ROOT/src -I $ROOT/tests -std=c++11  -DMALLOCTYPE=\"normal\""
libs="-pthread -lcityhash"

$CC -DOMVCC=true  $flags  -o "$EXE_DIR/mvccBanking-$numThr-$p.out"  $src $libs
$CC $ww           $flags  -o "$EXE_DIR/mv3cBanking-$numThr-$p.out"  $src $libs

#!/bin/bash
SCRIPT_DIR="$(dirname "$0")"
EXE_DIR="$(dirname "$0")/../output/executable/8b"
RAW_DIR="$(dirname "$0")/../output/raw"

rm -rf "$EXE_DIR/*"
rm -f header out
echo "TPCC  conflict test "

wares="1 2 3 4 6 8 10 12 14 16"
iterations=10

#Compile
for numThr in 10  
do
for p in  $wares
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 "$SCRIPT_DIR/compileTPCC.sh" 8b
done

rm ~/TStore/CavCommands* #FIX
cp ~/Full/CavCommands* ~/TStore/  #FIX

#MVCC
for numThr in 10
do
for p in $wares
do
for i in `seq $iterations`
do
sudo "$EXE_DIR/mvccTPCC-$numThr-$p.out"
done
done
done


#MV3C
for numThr in 10
do
for p in $wares
do
for i in `seq $iterations`
do
sudo "$EXE_DIR/mv3cTPCC-$numThr-$p.out"
done
done
done

cat header out > "$RAW_DIR/8b.csv"
rm -f header out




#!/bin/bash
SCRIPT_DIR="$(dirname "$0")"
EXE_DIR="$(dirname "$0")/../output/executable/7b"
RAW_DIR="$(dirname "$0")/../output/raw"

rm -rf "$EXE_DIR/*"
rm -f header out

echo "Banking  conflict test"

numThr=10
pList="0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0"
iterations=10

#Compile
for p in $pList
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 "$SCRIPT_DIR/compileBanking.sh" 7b


#MVCC
for p in $pList
do
for i in `seq $iterations`
do
sudo "$EXE_DIR/mvccBanking-$numThr-$p.out"					
done
done


#MV3C
for p in $pList
do
for i in `seq $iterations`
do
sudo "$EXE_DIR/mv3cBanking-$numThr-$p.out"					
done
done

cat header out > "$RAW_DIR/7b.csv"
rm -f header out

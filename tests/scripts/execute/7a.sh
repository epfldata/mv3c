#!/bin/bash
SCRIPT_DIR="$(dirname "$0")"
EXE_DIR="$(dirname "$0")/../output/executable/7a"
RAW_DIR="$(dirname "$0")/../output/raw"

rm -rf "$EXE_DIR/*"
rm -f header out
echo "Banking  thread test"
p=1.0

threads="1 2 3 4 5 6 7 8 9 10 11"
iterations=10

#Compile
for numThr in  $threads
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10  "$SCRIPT_DIR/compileBanking.sh" 7a

#MVCC
for numThr in  $threads
do
for i in `seq $iterations`
do
sudo "$EXE_DIR/mvccBanking-$numThr-$p.out"
done
done

#MV3C
for numThr in  $threads
do
for i in `seq $iterations`
do
sudo "$EXE_DIR/mv3cBanking-$numThr-$p.out"
done
done

 
cat header out > "$RAW_DIR/7a.csv"
rm -f header out

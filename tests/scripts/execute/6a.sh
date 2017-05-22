#!/bin/bash
SCRIPT_DIR="$(dirname "$0")"
EXE_DIR="$(dirname "$0")/../output/executable/6a"
RAW_DIR="$(dirname "$0")/../output/raw"

rm -rf "$EXE_DIR/*"
rm -f header out
echo "Trading thread test"
p=1.4
threads="1 2 3 4 5 6 7 8 9 10 11"
iterations=10

#Compile
for numThr in $threads
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 "$SCRIPT_DIR/compileTrading.sh" 6a


#MVCC
for numThr in $threads
do
for i in `seq $iterations`
do
sudo "$EXE_DIR/mvccTrading-$numThr-$p.out"		
done
done


#MV3C
for numThr in $threads
do
for i in `seq $iterations`
do
sudo "$EXE_DIR/mv3cTrading-$numThr-$p.out"
done
done

cat header out > "$RAW_DIR/6a.csv"
rm -f header out



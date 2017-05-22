#!/bin/bash
SCRIPT_DIR="$(dirname "$0")"
EXE_DIR="$(dirname "$0")/../output/executable/6b"
RAW_DIR="$(dirname "$0")/../output/raw"

rm -rf "$EXE_DIR/*"
rm -f header out

echo "Trading conflict test"

numThr=10
pList="1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.1 2.2 2.3 2.4"
iterations=10

#Compile
for p in $pList
do
echo -n "$numThr $p " 
done | xargs  -n 2 -P10 "$SCRIPT_DIR/compileTrading.sh" 6b

#MVCC
for p in $pList
do
for i in `seq $iterations`
do
sudo "$EXE_DIR/mvccTrading-$numThr-$p.out"			
done
done


#MV3C
for p in $pList
do
for i in `seq $iterations`
do	
sudo "$EXE_DIR/mv3cTrading-$numThr-$p.out"		
done
done


cat header out > "$RAW_DIR/6b.csv"
rm -f header out

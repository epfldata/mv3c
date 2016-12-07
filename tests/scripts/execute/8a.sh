#!/bin/bash
SCRIPT_DIR="$(dirname "$0")"
EXE_DIR="$(dirname "$0")/../output/executable/8a"
RAW_DIR="$(dirname "$0")/../output/raw"

rm -rf "$EXE_DIR/*"
rm -f header out
echo "TPCC  thread test "


threads="1 2 3 4 5 6 7 8 9 10 11"
iterations=10

#Compile
for p in 1 2
do
for numThr in  $threads
do
echo -n "$numThr $p " 
done
done | xargs  -n 2 -P10 "$SCRIPT_DIR/compileTPCC.sh" 8a

rm ~/TStore/CavCommands*   #FIX THIS
cp ~/Full/CavCommands* ~/TStore/  #FIX THIS

#MVCC 1
for p in 1
do
for numThr in  $threads
do
for i in `seq $iterations`
do
sudo "$EXE_DIR/mvccTPCC-$numThr-$p.out"
done
done
done


#MV3C 1
for p in 1
do
for numThr in  $threads
do
for i in `seq $iterations`
do
sudo "$EXE_DIR/mv3cTPCC-$numThr-$p.out"
done
done
done

cat header out > "$RAW_DIR/8a1.csv"
rm -f header out



#MVCC 2
for p in 2
do
for numThr in  $threads
do
for i in `seq $iterations`
do
sudo "$EXE_DIR/mvccTPCC-$numThr-$p.out"
done
done
done


#MV3C 2
for p in 2
do
for numThr in  $threads
do
for i in `seq $iterations`
do
sudo "$EXE_DIR/mv3cTPCC-$numThr-$p.out"
done
done
done

cat header out > "$RAW_DIR/8a2.csv"
rm -f header out

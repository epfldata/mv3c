#!/bin/bash
SCRIPT_DIR="$(dirname "$0")"
EXE_DIR=`readlink -m "$SCRIPT_DIR/../output/executable/7b"`
RAW_DIR=`readlink -m "$SCRIPT_DIR/../output/raw"`

rm -rf "$EXE_DIR"
mkdir -p $EXE_DIR $RAW_DIR

rm -f header out

echo "Experiment : Banking-Conflict (Figure 7b)"

numThr=$((NUMCORES-2))
pList="0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0"
iterations=${NUMITERS:-5}

#Compile
echo "  Generating executables ... "
for p in $pList
do
	echo -n "$numThr $p " 
done | xargs  -n 2 -P $numThr "$SCRIPT_DIR/compileBanking.sh" 7b

echo "  Running with $numThr threads"

#MVCC
echo "  OMVCC:"
for p in $pList
do
	echo -n  "    Fraction = $p      Iterations -> "
	for i in `seq $iterations`
	do
		echo -n " $i "
		RUN "$EXE_DIR/mvccBanking-$numThr-$p.out"  /dev/null					
	done
	echo ""
done


#MV3C
echo "  MV3C:"
for p in $pList
do
	echo -n  "    Fraction = $p      Iterations -> "
	for i in `seq $iterations`
	do
		echo -n " $i "
		RUN "$EXE_DIR/mv3cBanking-$numThr-$p.out"  /dev/null					
	done
	echo ""
done
echo ""
cat header out > "$RAW_DIR/7b.csv"
rm -f header out

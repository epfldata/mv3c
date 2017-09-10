#!/bin/bash
SCRIPT_DIR="$(dirname "$0")"
EXE_DIR=`readlink -m "$SCRIPT_DIR/../output/executable/7a"`
RAW_DIR=`readlink -m "$SCRIPT_DIR/../output/raw"`

rm -rf "$EXE_DIR"
mkdir -p $EXE_DIR $RAW_DIR

rm -f header out
echo "Experiment : Banking-Thread (Figure 7a)"
p=1.0
maxThr=$((NUMCORES-1))
threads=`seq $maxThr`
iterations=${NUMITERS:-5}

#Compile
echo "  Generating executables ... "
for numThr in  $threads
do
	echo -n "$numThr $p " 
done | xargs  -n 2 -P $maxThr  "$SCRIPT_DIR/compileBanking.sh" 7a

echo "  Running with fraction of conflict $p"

#MVCC
echo "  OMVCC:"
for numThr in  $threads
do
	echo -n "    Thread = $numThr     Iterations ->"
	for i in `seq $iterations`
	do
		echo -n " $i "
		RUN "$EXE_DIR/mvccBanking-$numThr-$p.out"  /dev/null
	done
	echo ""
done

#MV3C
echo "  MV3C :"
for numThr in  $threads
do
	echo -n "    Thread = $numThr     Iterations ->"
	for i in `seq $iterations`
	do
		echo -n " $i "
		RUN "$EXE_DIR/mv3cBanking-$numThr-$p.out"  /dev/null
	done
	echo ""
done
echo ""
 
cat header out > "$RAW_DIR/7a.csv"
rm -f header out

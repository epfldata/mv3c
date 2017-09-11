#!/bin/bash
SCRIPT_DIR="$(dirname "$0")"
EXE_DIR=`readlink -m "$SCRIPT_DIR/../output/executable/8b"`
RAW_DIR=`readlink -m "$SCRIPT_DIR/../output/raw"`
ROOT_DIR=`readlink -m ../../`

export NUMXACTS=1000000

rm -rf "$EXE_DIR"
mkdir -p $EXE_DIR $RAW_DIR
rm -f header out
echo "Experiment : TPCC-Conflict (Figure 8c)"

wares="1 2 3 4 6 8 10 12 14 16"
iterations=${NUMITERS:-5}

numThr=$((NUMCORES-2))

#Compile
echo "  Generating MV3C and OMVCC executables ... "
for p in  $wares
do
	echo -n "$numThr $p " 
done | xargs  -n 2 -P $numThr "$SCRIPT_DIR/compileTPCC.sh" 8b


if [ $GENDATA -eq 1 ]; then
	for w in $wares; do
		$ROOT_DIR/bench/tpcc-cmd-log.sh $NUMXACTS $w $ROOT_DIR/commands$w.txt > /dev/null 2>&1 #Generate data for w
	done
fi

echo "  Running with $numThr threads"

#MVCC
echo "    OMVCC:"
for p in $wares
do
	echo -n "      NumWare = $p      Iterations -> "
	for i in `seq $iterations`
	do
		echo -n " $i "
		RUN "$EXE_DIR/mvccTPCC-$numThr-$p.out"  /dev/null
	done
	echo ""
done



#MV3C
echo "    MV3C:"
for p in $wares
do
	echo -n "      NumWare = $p      Iterations -> "
	for i in `seq $iterations`
	do
		echo -n " $i "
		RUN "$EXE_DIR/mv3cTPCC-$numThr-$p.out"  /dev/null
	done
	echo ""
done

cat header out > "$RAW_DIR/8b.csv"
rm -f header out


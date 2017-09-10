#!/bin/bash
SCRIPT_DIR="$(dirname "$0")"
EXE_DIR=`readlink -m "$SCRIPT_DIR/../output/executable/6b"`
RAW_DIR=`readlink -m "$SCRIPT_DIR/../output/raw"`

rm -rf "$EXE_DIR"
mkdir -p $EXE_DIR $RAW_DIR
rm -f header out

echo "Experiment : Trading-Conflict (Figure 6b)"

numThr=$((NUMCORES-2))
alphaList="1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.1 2.2 2.3 2.4"
iterations=${NUMITERS:-5}

echo "  Generating executables ... "
#Compile
for alpha in $alphaList
do
	echo -n "$numThr $alpha " 
done | xargs  -n 2 -P $numThr "$SCRIPT_DIR/compileTrading.sh" 6b

echo "  Running with $numThr threads"

#MVCC
echo "  OMVCC:"
for alpha in $alphaList
do
	echo -n  "    Alpha = $alpha      Iterations -> "
	for i in `seq $iterations`
	do
		 echo -n " $i "
		 RUN "$EXE_DIR/mvccTrading-$numThr-$alpha.out" /dev/null		
	done
	echo ""
done


#MV3C
echo "   MV3C:"
for alpha in $alphaList
do
	echo -n  "    Alpha = $alpha      Iterations -> "
	for i in `seq $iterations`
	do	
		echo -n " $i "
		RUN "$EXE_DIR/mv3cTrading-$numThr-$alpha.out" /dev/null	
	done
	echo ""
done
echo ""

cat header out > "$RAW_DIR/6b.csv"
rm -f header out

#!/bin/bash
SCRIPT_DIR="$(dirname "$0")"
EXE_DIR=`readlink -m "$SCRIPT_DIR/../output/executable/6a"`
RAW_DIR=`readlink -m "$SCRIPT_DIR/../output/raw"`

rm -rf "$EXE_DIR"
mkdir -p $EXE_DIR $RAW_DIR
rm -f header out
echo "Experiment : Trading-Thread (Figure 6a)"

alpha=1.4
iterations=${NUMITERS:-5}
maxThreads=$((NUMCORES-1))
threads=`seq $maxThreads`

#Compile
echo "  Generating executables ..."
for numThr in $threads
do
	echo -n "$numThr $alpha " 
done | xargs  -n 2 -P $maxThreads "$SCRIPT_DIR/compileTrading.sh" 6a
echo "  Running with Alpha = $alpha"

#OMVCC
echo "  OMVCC :"
for numThr in $threads
do
	echo -n "    Thread = $numThr     Iterations ->"
	for i in `seq $iterations`
	do
		echo -n " $i "
	    RUN "$EXE_DIR/mvccTrading-$numThr-$alpha.out" /dev/null		
	done
	echo ""
done


#MV3C
echo "  MV3C :"
for numThr in $threads
do
	echo -n "    Thread = $numThr     Iterations ->"
	for i in `seq $iterations`
	do
	  echo -n " $i "
	 RUN "$EXE_DIR/mv3cTrading-$numThr-$alpha.out"  /dev/null
	done
	echo ""
done
echo ""

cat header out > "$RAW_DIR/6a.csv"
rm -f header out
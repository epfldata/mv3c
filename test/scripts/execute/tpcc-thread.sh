#!/bin/bash
SCRIPT_DIR="$(dirname "$0")"
EXE_DIR=`readlink -m "$SCRIPT_DIR/../output/executable/8a"`
RAW_DIR=`readlink -m "$SCRIPT_DIR/../output/raw"`
ROOT_DIR=`readlink -m ../..`

export NUMXACTS=1000000

rm -rf "$EXE_DIR"
mkdir -p $EXE_DIR $RAW_DIR
rm -f header out

echo "Experiment : TPCC-Thread (Figures 8a 8b)"
maxThr=$((NUMCORES-1))
threads=`seq $maxThr`
iterations=${NUMITERS:-5}

if [ $GENDATA -eq 1 ]; then
	echo "  Generating fresh commands ... "
	$ROOT_DIR/bench/tpcc-cmd-log.sh $NUMXACTS 1 $ROOT_DIR/commands1.txt > /dev/null #Generate data for w 1
	$ROOT_DIR/bench/tpcc-cmd-log.sh $NUMXACTS 2 $ROOT_DIR/commands2.txt > /dev/null #Generate data for w 2
fi


#Compile
echo "  Generating MV3C and OMVCC executables ... "
for p in 1 2
do
	for numThr in  $threads
		do
		echo -n "$numThr $p " 
	done
done | xargs  -n 2 -P $maxThr "$SCRIPT_DIR/compileTPCC.sh" 8a

echo "  Running with NumWare = 1"
#MVCC 1
echo "    OMVCC:"
for p in 1
do
	
	for numThr in  $threads
	do
		echo -n "      Thread = $numThr     Iterations ->"
		for i in `seq $iterations`
		do
			echo -n " $i "
			RUN "$EXE_DIR/mvccTPCC-$numThr-$p.out"  /dev/null
		done
		echo ""
	done
done

#MV3C 1
echo "    MV3C:"
for p in 1
do
	for numThr in  $threads
	do
		echo -n "      Thread = $numThr     Iterations ->"
		for i in `seq $iterations`
		do
			echo -n " $i "
			RUN "$EXE_DIR/mv3cTPCC-$numThr-$p.out"  /dev/null
		done
		echo ""
	done
done

cat header out > "$RAW_DIR/8a1.csv"
rm -f header out


echo "  Running with NumWare = 2"
echo "    OMVCC:"
#MVCC 2
for p in 2
do
	for numThr in  $threads
	do
		echo -n "      Thread = $numThr     Iterations ->"
		for i in `seq $iterations`
		do
			echo -n " $i "
			RUN "$EXE_DIR/mvccTPCC-$numThr-$p.out"  /dev/null
		done
		echo ""
	done
done




#MV3C 2
echo "    MV3C:"
for p in 2
do
	for numThr in  $threads
	do
		echo -n "      Thread = $numThr     Iterations ->"
		for i in `seq $iterations`
		do
			echo -n " $i "
			RUN "$EXE_DIR/mv3cTPCC-$numThr-$p.out"  /dev/null
		done
		echo ""
	done
done

cat header out > "$RAW_DIR/8a2.csv"
rm -f header out

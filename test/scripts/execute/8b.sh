#!/bin/bash
SCRIPT_DIR="$(dirname "$0")"
EXE_DIR=`readlink -m "$SCRIPT_DIR/../output/executable/8b"`
RAW_DIR=`readlink -m "$SCRIPT_DIR/../output/raw"`
ST_DIR=`readlink -m ../../../MV3C_SingleThreaded`
CAV2_DIR=`readlink -m ../../../ThirdParty/Cavalia-MV3C`
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
	echo "  Generating fresh commands ... "
	$CAV2_DIR/genCommands.sh $ST_DIR > /dev/null #Generate fresh commands from Cavalia
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


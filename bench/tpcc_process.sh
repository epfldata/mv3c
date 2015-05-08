#!/bin/bash

dirp="$(dirname "$0")"

echo "impl,W,exec_time,run1,run2,run3,run4,run5" > "$dirp/results/tpcc/"sstore.csv
for f in "$dirp/results/tpcc/"sstore_*.txt
do
	echo "$f"
	line=`cat $f | tail -n 3 | head -n 1`
	line=${line:15}
	
	w=$(basename $f)
	w=${w:7}
	w=`echo $w|rev`
	w=${w:8}
	w=`echo $w|rev`
	w=`echo "$w" | tr _ ,`
	
	echo "$w,${line%?}" >> "$dirp/results/tpcc/sstore.csv"
done

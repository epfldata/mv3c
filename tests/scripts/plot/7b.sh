#!/bin/bash 

INPUT_DIR=`readlink -f "$(dirname "$0")/../output/average"`
OUTPUT_DIR=`readlink -f "$(dirname "$0")/../output/graphs"`
mkdir -p $OUTPUT_DIR
FILE="$INPUT_DIR/7b.csv"

gnuplot << EOF
set datafile separator ","
set terminal pdf size 12cm,5cm
set termoption enhanced
set output "$OUTPUT_DIR/banking-percentage.pdf"

set lmargin at screen 0.18;
set rmargin at screen 0.99;
set bmargin at screen 0.2;
set tmargin at screen 0.9;

set size ratio 0.33
set auto y
set auto x
set grid ytics
set ytics 200

set border 3
set tics scale 0
set key font ",16"

set ylabel "{/Times-Bold=20 Throughput\n(kilo transactions/second)}"
set xlabel "{/Times-Bold=20 Percentage of conflict in concurrent transactions}"

set style data histogram
set style histogram cluster gap 1
set boxwidth 0.8

plot '$FILE' using 2:xtic(1) lc rgb"#A74A44" fs pattern 2 border rgb "#77342E" title col, '$FILE' using 3:xtic(1) lc rgb"#89A24C" fs solid border rgb"#6C7F3C" title col
EOF
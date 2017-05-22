#!/bin/bash 

INPUT_DIR="$(dirname "$0")/../output/average"
OUTPUT_DIR="$(dirname "$0")/../output/graphs"
FILE="$INPUT_DIR/6b.csv"

gnuplot << EOF
set datafile separator ","
set terminal pdf size 12cm,5cm
set output "$OUTPUT_DIR/trading-percentage.pdf"

set lmargin at screen 0.2;
set rmargin at screen 0.99;
set bmargin at screen 0.28;
set tmargin at screen 0.9;

set size ratio 0.33
set auto y
set auto x
set ytics 200
set tics scale 0

set border 3
set grid ytics


set key font ",16"

set ylabel "{/Times-Bold=16 Throughput\n(kilo tuples/second)}"
set xlabel "{/Times-Bold=16 Zipf parameter (α) for s_id distribution }"

set style data histogram
set style histogram cluster gap 1
set boxwidth 0.8

plot '$FILE' using 2:xtic(1) lc rgb"#A74A44" fs pattern 2 border rgb "#77342E" title col, '$FILE' using 3:xtic(1) lc rgb"#89A24C" fs solid border rgb"#6C7F3C" title col
EOF
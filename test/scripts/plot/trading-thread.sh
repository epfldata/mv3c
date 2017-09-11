#!/bin/bash 

INPUT_DIR=`readlink -m "$(dirname "$0")/../output/average"`
OUTPUT_DIR=`readlink -m "$(dirname "$0")/../output/graphs"`
OUTFILE="$OUTPUT_DIR/trading-window.pdf"
mkdir -p $OUTPUT_DIR
echo "  Saving result graph as $OUTFILE"
echo " "
FILE="$INPUT_DIR/6a.csv"
gnuplot << EOF
set datafile separator ","
set terminal pdf size 12cm,5cm
set output "$OUTFILE"

set lmargin at screen 0.2;
set rmargin at screen 0.95;
set bmargin at screen 0.28;
set tmargin at screen 0.999;

set size ratio 0.33
set auto y
set auto x
set xtics 1
set ytics 200
set tics scale 0

set border 3

set key left font ",16"

set style line 1 lt rgb "#A74A44" lw 1 pt 7
set style line 2 lt rgb "#89A24C" lw 3 pt 9

set grid ytics
set xlabel "{/Times-Bold=16 # of worker threads}"
set ylabel "{/Times-Bold=16 Throughput}\n(kilo transactions/second)"

plot '$FILE' using 1:2  w linespoints ls 1 title columnheader(2) , '$FILE' using 1:3 w linespoints ls 2 title columnheader(3)
EOF
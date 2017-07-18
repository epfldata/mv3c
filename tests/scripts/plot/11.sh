#!/bin/bash 

INPUT_DIR=`readlink -f "$(dirname "$0")/../output/average"`
OUTPUT_DIR=`readlink -f "$(dirname "$0")/../output/graphs"`
mkdir -p $OUTPUT_DIR
FILE="$INPUT_DIR/11.csv"

gnuplot << EOF
set datafile separator ","
set terminal pdf size 12cm,5cm
set output "$OUTPUT_DIR/tpcc_w1.pdf"

set lmargin at screen 0.18;
set rmargin at screen 0.95;
set bmargin at screen 0.25;
set tmargin at screen 0.999999;

set size ratio 0.33
set auto y
set auto x
set xrange [10:64]

set ytics 10
set tics scale 0

set border 3
set key font ",16"

set style line 1 lt rgb "#A74A44" lw 1 pt 7
set style line 2 lt rgb "#89A24C" lw 3 pt 9

set grid ytics
set xlabel "{/Times-Bold=20 # of concurrent transactions}"
set ylabel "{/Times-Bold=20 Throughput \n (kilo transactions/second)}"

plot '$FILE' using 1:2  w linespoints ls 1 title columnheader(2) , '$FILE' using 1:3 w linespoints ls 2 title columnheader(3)
EOF

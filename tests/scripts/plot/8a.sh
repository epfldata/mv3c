#!/bin/bash 

INPUT_DIR=`readlink -f "$(dirname "$0")/../output/average"`
OUTPUT_DIR=`readlink -f "$(dirname "$0")/../output/graphs"`
mkdir -p $OUTPUT_DIR
FILE1="$INPUT_DIR/8a1.csv"
FILE2="$INPUT_DIR/8a2.csv"

gnuplot << EOF
set datafile separator ","
set terminal pdf size 10cm,4.7cm
set output "$OUTPUT_DIR/tpcc-8a-w1-full.pdf"

set lmargin at screen 0.2;
set rmargin at screen 0.95;
set bmargin at screen 0.22;
set tmargin at screen 0.8;

set size ratio 0.33
set auto y
set auto x
set xtics 1
set ytics 50

set tics scale 0
set border 3
set key tmargin horizontal font ",16" 

set style line 1 lt rgb "#A74A44" lw 1 pt 7
set style line 2 lt rgb "#89A24C" lw 3 pt 9
set style line 3 lt rgb "#66A1DD" lw 1 pt 5
set style line 4 lt rgb "#524265" lw 3 pt 3

set grid ytics

set xlabel "{/Times-Bold=20 # of worker threads}"
set ylabel "{/Times-Bold=20 Throughput\n(kilo transactions/second)}"

plot '$FILE1' using 1:2  w linespoints ls 1 title columnheader(2) , '$FILE1' using 1:3 w linespoints ls 2 title columnheader(3) , '$FILE1' using 1:4 w linespoints ls 3 title columnheader(4) , '$FILE1' using 1:5 w linespoints ls 4 title columnheader(5)


set output "$OUTPUT_DIR/tpcc-8a-w2-full.pdf"
plot '$FILE2' using 1:2  w linespoints ls 1 title columnheader(2) , '$FILE2' using 1:3 w linespoints ls 2 title columnheader(3) , '$FILE2' using 1:4 w linespoints ls 3 title columnheader(4) , '$FILE2' using 1:5 w linespoints ls 4 title columnheader(5)
EOF
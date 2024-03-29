#!/bin/bash 

INPUT_DIR=`readlink -m "$(dirname "$0")/../output/average"`
OUTPUT_DIR=`readlink -m "$(dirname "$0")/../output/graphs"`
mkdir -p $OUTPUT_DIR
OUTFILE1="$OUTPUT_DIR/tpcc-8a-w1-full.pdf"
OUTFILE2="$OUTPUT_DIR/tpcc-8a-w2-full.pdf"
FILE1="$INPUT_DIR/8a1.csv"
FILE2="$INPUT_DIR/8a2.csv"
echo "  Saving result graphs as $OUTFILE1 and $OUTFILE2"
echo " "
gnuplot << EOF
set datafile separator ","
set terminal pdf size 10cm,4.7cm
set output "$OUTFILE1"

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
set ylabel "{/Times-Bold=20 Throughput}\n(kilo transactions/second)"

plot '$FILE1' using 1:2  w linespoints ls 1 title columnheader(2) , '$FILE1' using 1:3 w linespoints ls 2 title columnheader(3)


set output "$OUTFILE2"
plot '$FILE2' using 1:2  w linespoints ls 1 title columnheader(2) , '$FILE2' using 1:3 w linespoints ls 2 title columnheader(3)
EOF

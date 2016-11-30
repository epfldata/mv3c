name = '8a1-thr'
set datafile separator ","
set terminal pdf size 10cm,4.7cm
set output "tpcc-8a-w1-full.pdf"
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
set ylabel "{/Times-Bold=20 Throughput\n(kilo tuples/second)}"
plot name.".avg" using 1:2  w linespoints ls 1 title columnheader(2) , name.".avg" using 1:3 w linespoints ls 2 title columnheader(3) , name.".avg" using 1:4 w linespoints ls 3 title columnheader(4) , name.".avg" using 1:5 w linespoints ls 4 title columnheader(5)

name = '8a2-thr'
set output "tpcc-8a-w2-full.pdf"
plot name.".avg" using 1:2  w linespoints ls 1 title columnheader(2) , name.".avg" using 1:3 w linespoints ls 2 title columnheader(3) , name.".avg" using 1:4 w linespoints ls 3 title columnheader(4) , name.".avg" using 1:5 w linespoints ls 4 title columnheader(5)

name = '7a-thr'
set datafile separator ","
set terminal pdf size 12cm,5cm
set output "banking-window.pdf"
set lmargin at screen 0.18;
set rmargin at screen 0.95;
set bmargin at screen 0.25;
set tmargin at screen 0.999999;
set size ratio 0.33
set auto y
set auto x
set xtics 1
set ytics 500
set tics scale 0
set border 3
set key font ",16"
set style line 1 lt rgb "#A74A44" lw 1 pt 7
set style line 2 lt rgb "#89A24C" lw 3 pt 9
set grid ytics
set xlabel "{/Times-Bold=20 # of worker threads}"
set ylabel "{/Times-Bold=20 Throughput \n (kilo tuples/second)}"
plot name.".avg" using 1:2  w linespoints ls 1 title columnheader(2) , name.".avg" using 1:3 w linespoints ls 2 title columnheader(3)

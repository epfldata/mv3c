name = '6a-st'
set datafile separator ","
set auto y
set auto x
set xtics 1
set offset graph 0, graph 0, graph 0.2, graph 0
set style line 1 lt rgb "blue" lw 3 pt 7
set style line 2 lt rgb "red" lw 3 pt 9
set grid ytics
set xlabel "# of worker threads"
set ylabel "Time to process 5 million transactions"
set terminal pdf
set output name.".pdf"
plot name.".avg" using 1:2  w linespoints ls 1 title columnheader(2) , name.".avg" using 1:3 w linespoints ls 2 title columnheader(3)

name = '7b-st'
set datafile separator ","
set auto y
set auto x
set grid ytics
set offset graph 0, graph 0, graph 0.2, graph 0
set xlabel "Percentage of  conflict in concurrent transactions"
set ylabel "Time to process 5 million transactions"
set style data histogram
set style histogram cluster gap 1
set style fill solid border rgb"black"
set boxwidth 0.9
set terminal pdf
set output name.".pdf"
plot name.".avg" using 2:xtic(1) lc rgb"#A74A44" fs pattern 3 title col, name.".avg" using 3:xtic(1) lc rgb"#89A24C" fs pattern 2 title col
name = '6b'
set datafile separator ","
set auto y
set auto x
set grid ytics
set offset graph 0, graph 0, graph 0.2, graph 0.1
set ylabel "Time to process 5 million transactions"
set xlabel "Zipf parameter (α) for s_id distribution in TradeOrder and UpdatePrice"
set style data histogram
set style histogram cluster gap 1
set style fill solid border rgb"black"
set boxwidth 0.9
set terminal pdf
set output name.".pdf"
plot name.".avg" using 2:xtic(1) lc rgb"blue" fs pattern 3 title col, name.".avg" using 3:xtic(1) lc rgb"red" fs pattern 2 title col

name = '8b-thr'
set datafile separator ","
set terminal pdf size 10cm,4.7cm
set output "tpcc-8b-full.pdf"
set lmargin at screen 0.2;
set rmargin at screen 0.99;
set bmargin at screen 0.22;
set tmargin at screen 0.8;
set size ratio 0.33
set auto y
set auto x
set ytics 50
set border 3
set grid ytics
set key tmargin horizontal font ",16"
set tics scale 0
set ylabel "{/Times-Bold=20 Throughput\n(kilo tuples/second)}"
set xlabel "{/Times-Bold=20 # of warehouses in TPC-­C benchmark}"
set style data histogram
set style histogram cluster gap 1
set boxwidth 0.8
plot name.".avg" using 2:xtic(1) lc rgb"#A74A44" fs pattern 2 border rgb "#77342E" title col, name.".avg" using 3:xtic(1) lc rgb"#89A24C" fs solid border rgb"#6C7F3C" title col, name.".avg" using 4:xtic(1) lc rgb"#66A1DD" fs pattern 6 border rgb"#295177" title col, name.".avg" using 5:xtic(1) lc rgb"#524265" fs pattern 7 border rgb"#564769" title col


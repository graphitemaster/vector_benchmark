set terminal png size 512,256 enhanced
set output 'results.png'
# set lmargin 1
# set rmargin 0
# set tmargin 0
# set bmargin 0
set yrange [0:2]
set ylabel "Time (seconds)"
set style data histogram
set style histogram cluster gap 1
set style fill solid
set boxwidth 1
set xtics format ""
set xtic scale 0.9
set grid ytics

set title "std::vector<T> vs Array<T>"
plot "results.dat" using 2:xtic(1) title "pod", \
     "results.dat" using 3 title "trivial", \
     "results.dat" using 4 title "non-trivial"
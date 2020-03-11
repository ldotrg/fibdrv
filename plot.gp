reset

# Usege :
# gnuplot -e 'in="performance.csv";out="fibtime.png";gtitle="Fibonacci Sequence Performance"' plot.gp

set output out
set title gtitle noenhanced

set xlabel 'Nth Sequence'
set ylabel 'time(nsec)'
set terminal png font " Times_New_Roman,14 " truecolor size 1200,700
# Position & format of label has been carefully adjusted,
# do not change unless you are sure

set label 1 at screen 0.92, screen 0.6 left
set key left
set grid
set datafile separator ","
plot \
in using 1:2 with linespoints linewidth 2 title "BaseLine"
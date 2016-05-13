set datafile sep ","
set decimal locale
set format y "%'.0f"

set title "Example 1"
set xlabel "Time / Years"
set ylabel "Exposure"

set autoscale

set style line 1 linecolor rgb '#0060ad' linetype 1 linewidth 3
set style line 2 linecolor rgb '#dd181f' linetype 1 linewidth 3
set style line 3 linecolor rgb '#008000' linetype 1 linewidth 3

set term pdfcairo 
set out "tmp.pdf"
plot "exposure_trade_Swap_20y.csv" us 3:4 title "EPE" w l ls 1
replot "exposure_trade_Swap_20y.csv" us 3:5 title "ENE" w l ls 2
replot "swaption_npv.csv" us 4:5 title "Swaptions" w linesp ls 3 pt 4 ps 0.5 

set out "plot.pdf"
replot

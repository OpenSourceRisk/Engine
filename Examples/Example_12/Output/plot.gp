set datafile sep ","
set decimal locale
set format y "%'.0f"

set title "Example 1b"
set xlabel "Time / Years"
set ylabel "Exposure"

set autoscale
set yrange [0:2500000]

set style line 1 linecolor rgb '#0060ad' linetype 1 linewidth 0.5
set style line 2 linecolor rgb '#dd181f' linetype 1 linewidth 0.5
set style line 3 linecolor rgb '#008000' linetype 1 linewidth 1
set style line 4 linecolor rgb 'blue' linetype 1 linewidth 2
set style line 5 linecolor rgb 'violet' linetype 1 linewidth 2

set term pdfcairo 
set out "tmp.pdf"
cd "~/openxva/Examples/Example_12/Output"
plot "exposure_trade_Swap_50y.csv" us 3:4 title "EPE (no horizon shift)" w l ls 1
replot "exposure_trade_Swap_50y.csv" us 3:5 title "ENE (no horizon shift)" w l ls 2
replot "exposure_trade_Swap_50y_2.csv" us 3:4 title "EPE (shifted horizon)" w l ls 4
replot "exposure_trade_Swap_50y_2.csv" us 3:5 title "ENE (shifted horizon)" w l ls 5
replot "swaption_npv.csv" us 4:5 title "Swaptions" w linesp ls 3 pt 4 ps 0.5 

set out "plot.pdf"
replot

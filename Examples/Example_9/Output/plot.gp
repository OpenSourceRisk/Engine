set datafile sep ","
set decimal locale
set format y "%'.0f"

set title "Example 9"
set xlabel "Time / Years"
set ylabel "Exposure"

set autoscale

set style line 1 linecolor rgb '#0060ad' linetype 1 linewidth 3
set style line 2 linecolor rgb '#dd181f' linetype 1 linewidth 3

set term pdfcairo 
set out "tmp.pdf"
plot "exposure_trade_10_Swap_EUR_USD.csv" us 3:4 title "Swap" w l ls 1
replot "exposure_trade_10_Swap_EUR_USD_RESET.csv" us 3:4 title "Resettable Swap" w l ls 2

set out "plot.pdf"
replot

set datafile sep ","
set decimal locale
set format y "%'.0f"

set title "Example 6"
set xlabel "Time / Years"
set ylabel "Exposure"

set style line 1 linecolor rgb '#0060ad' linetype 1 linewidth 3
set style line 2 linecolor rgb '#dd181f' linetype 1 linewidth 3
set style line 3 linecolor rgb '#008000' linetype 1 linewidth 3
set style line 4 linecolor rgb '#990099' linetype 1 linewidth 3

set autoscale

set term pdfcairo 
set out "tmp.pdf"
plot "exposure_trade_Swap.csv" us 3:4 title "EPE Swap" w l ls 1
replot "exposure_trade_Swaption.csv" us 3:5 title "ENE Swaption" w l ls 2
replot "exposure_nettingset_CTPY_A.csv" us 3:4 title "EPE Netting Set" w l ls 3
replot "exposure_trade_ShortSwap.csv" us 3:4 title "EPE Short Swap" w l ls 4

set out "plot_callable_swap.pdf"
replot


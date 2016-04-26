set datafile sep ","
set decimal locale
set format y "%'.0f"

set xlabel "Time / Years"
set ylabel "Exposure"

plot "exposure_trade_204128.csv" us 3:4 title "EPE Swap" w l lw 3
replot "exposure_trade_204128-swaption.csv" us 3:5 title "ENE Swaption" w l lw 3
replot "exposure_nettingset_CUST_A.csv" us 3:4 title "EPE Netting Set" w l lw 3
replot "exposure_trade_204128-short.csv" us 3:4 title "EPE Short Swap" w l lw 3

set term pdfcairo 
set out "plot_callable_swap.pdf"
replot
set term aqua


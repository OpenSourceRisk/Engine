set datafile sep ","
set decimal locale
set format y "%'.0f"

set title "Example 2"
set xlabel "Time / Years"
set ylabel "Exposure"

set autoscale

plot "exposure_trade_FXFWD_EURUSD_10Y.csv" us 3:4 title "EPE" w l lw 3
replot "exposure_trade_FXFWD_EURUSD_10Y.csv" us 3:5 title "ENE" w l lw 3
replot "call.csv" us 1:2 title "Call Price" w l lw 3
replot "put.csv" us 1:2 title "Put Price" w l lw 3

set term pdfcairo 
set out "plot.pdf"
replot
set term aqua

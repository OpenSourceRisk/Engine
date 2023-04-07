set datafile sep ","
set decimal locale
set format y "%'.0f"

set xlabel "Time / Years"
set ylabel "Exposure"

set autoscale

#set yrange [0:100000]
plot "exposure_trade_Swap_20.csv" us 3:4 title "EPE" w l lw 3
replot "exposure_trade_Swap_20.csv" us 3:5 title "ENE" w l lw 3
replot "swaption_npv.csv" us 4:5 title "Swaptions" w linesp lw 3

set term pdfcairo 
set out "gnu.pdf"
replot
set term aqua

set datafile sep ","
set decimal locale
set format y "%'.0f"

set xlabel "Time / Years"
set ylabel "Exposure"

plot "exposure_trade_Swap_20.csv" us 3:4 title "EPE" w l lw 3
replot "exposure_trade_Swap_20.csv" us 3:5 title "ENE" w l lw 3
replot "npv_payer.csv" us 4:5 title "Payer Swaption" w linesp lw 3
replot "npv_receiver.csv" us 4:5 title "Receiver Swaption" w linesp lw 3

set term pdfcairo 
set out "gnu.pdf"
replot
set term aqua
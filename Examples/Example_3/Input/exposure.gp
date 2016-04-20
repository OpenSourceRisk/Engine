set datafile sep ","
plot "exposure_trade_Swap_20.csv" us 3:4 title "EPE" w l lw 3
replot "exposure_trade_Swap_20.csv" us 3:5 title "ENE" w l lw 3
replot "npv.csv" us 4:5 title "Swaption" w linesp lw 3

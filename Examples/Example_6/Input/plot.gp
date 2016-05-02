set datafile sep ","
set decimal locale
set format y "%'.0f"

set xlabel "Time / Years"
set ylabel "Exposure"

set yrange [0:20000]
#plot "exposure_trade_70309.csv" us 3:4 title "EPE 70309" w l lw 3
#replot "exposure_trade_70309.csv" us 3:5 title "ENE 70309" w l lw 3

#replot "exposure_trade_919020.csv" us 3:4 title "EPE 919020" w l lw 3
#replot "exposure_trade_919020.csv" us 3:5 title "ENE 919020" w l lw 3

#replot "exposure_trade_938498.csv" us 3:4 title "EPE 938498" w l lw 3
#replot "exposure_trade_938498.csv" us 3:5 title "ENE 938498" w l lw 3

plot "exposure_nettingset_CUST_A.csv" us 3:4 title "EPE Netting Set" w l lw 3
replot "exposure_nettingset_CUST_A.csv" us 3:5 title "ENE Netting Set" w l lw 3

set term pdfcairo 
set out "gnu.pdf"
replot
set term aqua

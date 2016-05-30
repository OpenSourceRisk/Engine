set datafile sep ","
set decimal locale
set format y "%'.0f"

set title "Example 5"
set xlabel "Time / Years"
set ylabel "Exposure"

set style line 1 linecolor rgb '#0060ad' linetype 1 linewidth 3
set style line 2 linecolor rgb '#dd181f' linetype 1 linewidth 3
set style line 3 linecolor rgb '#008000' linetype 1 linewidth 3
set style line 4 linecolor rgb '#990099' linetype 1 linewidth 3

set autoscale
set term pdfcairo

set out "tmp.pdf"
plot "collateral_none/exposure_trade_70309.csv" us 3:4 title "EPE 70309" w l ls 1
replot "collateral_none/exposure_trade_919020.csv" us 3:4 title "EPE 919020" w l ls 2
replot "collateral_none/exposure_trade_938498.csv" us 3:4 title "EPE 938498" w l ls 3
replot "collateral_none/exposure_nettingset_CUST_A.csv" us 3:4 title "EPE NettingSet" w l ls 4
set out "plot_nocollateral_epe.pdf"
replot

set out "tmp.pdf"
plot "collateral_none/exposure_trade_70309.csv" us 3:5 title "ENE 70309" w l ls 1
replot "collateral_none/exposure_trade_919020.csv" us 3:5 title "ENE 919020" w l ls 2
replot "collateral_none/exposure_trade_938498.csv" us 3:5 title "ENE 938498" w l ls 3
replot "collateral_none/exposure_nettingset_CUST_A.csv" us 3:5 title "ENE NettingSet" w l ls 4
set out "plot_nocollateral_ene.pdf"
replot

set out "tmp.pdf"
plot "collateral_none/exposure_trade_70309.csv" us 3:6 title "Allocated EPE 70309" w l ls 1
replot "collateral_none/exposure_trade_919020.csv" us 3:6 title "Allocated EPE 919020" w l ls 2
replot "collateral_none/exposure_trade_938498.csv" us 3:6 title "Allocated EPE 938498" w l ls 3
set out "plot_nocollateral_allocated_epe.pdf"
replot

set out "tmp.pdf"
plot "collateral_none/exposure_trade_70309.csv" us 3:7 title "Allocated ENE 70309" w l ls 1
replot "collateral_none/exposure_trade_919020.csv" us 3:7 title "Allocated ENE 919020" w l ls 2
replot "collateral_none/exposure_trade_938498.csv" us 3:7 title "Allocated ENE 938498" w l ls 3
set out "plot_nocollateral_allocated_ene.pdf"
replot

set out "tmp.pdf"
plot "collateral_none/exposure_nettingset_CUST_A.csv" us 3:4 title "EPE NettingSet" w l ls 1
replot "collateral_threshold/exposure_nettingset_CUST_A.csv" us 3:4 title "EPE NettingSet, Threshold 1m" w l ls 2
set out "plot_threshold_epe.pdf"
replot

set out "tmp.pdf"
plot "collateral_threshold/exposure_trade_70309.csv" us 3:6 title "Allocated EPE 70309" w l ls 1
replot "collateral_threshold/exposure_trade_919020.csv" us 3:6 title "Allocated EPE 919020" w l ls 2
replot "collateral_threshold/exposure_trade_938498.csv" us 3:6 title "Allocated EPE 938498" w l ls 3
set out "plot_threshold_allocated_epe.pdf"
replot

set out "tmp.pdf"
plot "collateral_none/exposure_nettingset_CUST_A.csv" us 3:4 title "EPE NettingSet" w l ls 1
replot "collateral_mta/exposure_nettingset_CUST_A.csv" us 3:4 title "EPE NettingSet, MTA 100k" w l ls 2
set out "plot_mta_epe.pdf"
replot

set out "tmp.pdf"
plot "collateral_none/exposure_nettingset_CUST_A.csv" us 3:4 title "EPE NettingSet" w l ls 1
replot "collateral_mpor/exposure_nettingset_CUST_A.csv" us 3:4 title "EPE NettingSet, MPOR 2W" w l ls 2
set out "plot_mpor_epe.pdf"
replot

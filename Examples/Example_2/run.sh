#!/bin/bash

exe=../../App/ore

# run simulation and exposure postprocessor
echo ""
echo "1) Run ORE to produce NPV cube and exposures"
echo ""
$exe Input/ore.xml

# check times
echo ""
echo "Get times from the log file"
echo ""
cat Output/log.txt | grep "ValuationEngine completed" | cut -d":" -f6-

# run swaption pricing
echo ""
echo "2) Run ORE to price European Payer Swaptions"
echo ""
$exe Input/ore_payer_swaption.xml
cd Output
cat npv_payer.csv | awk -F"," ' { split($1,tokens,"_"); print tokens[2] "," $7; } ' > npv_payer_2.csv
cd ..

# run swaption pricing
echo ""
echo "3) Run ORE to price European Receiver Swaptions"
echo ""
$exe Input/ore_receiver_swaption.xml
cd Output
cat npv_receiver.csv | awk -F"," ' { split($1,tokens,"_"); print tokens[2] "," $7; } ' > npv_receiver_2.csv
cd ..

# plot
echo ""
echo "4) Plot results: Simulated exposures vs analytical Swaption prices"
echo ""
cd Output
gnuplot plot.gp

echo ""
echo "View Output/plot.pdf"
echo ""

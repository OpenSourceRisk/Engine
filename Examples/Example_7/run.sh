#!/bin/bash

exe=../../App/ore

# run simulation and exposure postprocessor
echo ""
echo "1) Run ORE to produce NPV cube and exposures"
echo ""
$exe Input/ore.xml

# check times
times=`cat Output/log.txt | grep "ValuationEngine completed" | cut -d":" -f6-`
echo ""
echo "Times: $times"

# plot
echo ""
echo "3) Plot results: Simulated exposures vs analytical option prices"
echo ""
cd Output
cat npv.csv | grep CALL | awk -F"," ' { print 0 "," $7; print $4 "," $7; } ' > call.csv
cat npv.csv | grep PUT | awk -F"," ' { print 0 "," $7; print $4 "," $7; } ' > put.csv
gnuplot plot.gp
cd ..

echo ""
echo "View Output/*.pdf"
echo ""

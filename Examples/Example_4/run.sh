#!/bin/bash

exe=../../App/ore

# run simulation and exposure postprocessor
echo ""
echo "1) Run ORE to produce NPV cube and exposures, without collateral"
echo ""
$exe Input/ore.xml

# check times
times=`cat Output/log.txt | grep "ValuationEngine completed" | cut -d":" -f6-`
echo ""
echo "Times: $times"

# plot
echo ""
echo "2) Plot results"
echo ""
cd Output
gnuplot plot.gp

echo ""
echo "View Output/*.pdf"
echo ""

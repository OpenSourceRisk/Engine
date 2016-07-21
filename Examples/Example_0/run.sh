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


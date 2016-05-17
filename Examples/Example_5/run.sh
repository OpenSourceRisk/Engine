#!/bin/bash

exe=../../App/ore

# run simulation and exposure postprocessor
echo ""
echo "1) Run ORE to produce NPV cube and exposures, without collateral"
echo ""
$exe Input/ore.xml

mkdir Output/collateral_none
cp Output/exposure* Output/collateral_none
cp Output/xva.csv Output/collateral_none

# check times
echo ""
echo "Get times from the log file"
echo ""
cat Output/log.txt | grep "ValuationEngine completed" | cut -d":" -f6-

# run collateral postprocessor
echo ""
echo "2) Run ORE to postprocess the NPV cube, with collateral (threshold>0)"
echo ""
$exe Input/ore_threshold.xml

mkdir Output/collateral_threshold
cp Output/exposure* Output/collateral_threshold
cp Output/xva.csv Output/collateral_threshold
cp Output/colva* Output/collateral_threshold

# run collateral postprocessor
echo ""
echo "3) Run ORE to postprocess the NPV cube, with collateral (threshold=0)"
echo ""
$exe Input/ore_mta.xml

mkdir Output/collateral_mta
cp Output/exposure* Output/collateral_mta
cp Output/xva.csv Output/collateral_mta
cp Output/colva* Output/collateral_mta

# run collateral postprocessor
echo ""
echo "4) Run ORE to postprocess the NPV cube, with collateral (threshold=mta=0)"
echo ""
$exe Input/ore_mpor.xml

mkdir Output/collateral_mpor
cp Output/exposure* Output/collateral_mpor
cp Output/xva.csv Output/collateral_mpor
cp Output/colva* Output/collateral_mpor

# plot
echo ""
echo "5) Plot results"
echo ""
cd Output
gnuplot plot.gp

echo ""
echo "View Output/plot*.pdf"
echo ""

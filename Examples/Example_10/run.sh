#!/bin/bash

exe=../../App/ore

# run simulation and exposure postprocessor
echo ""
echo "1) Run ORE to produce NPV cube and exposures, without collateral"
echo ""
$exe Input/ore.xml

mkdir Output/collateral_none
cp Output/log.txt Output/collateral_none
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
cp Output/log.txt Output/collateral_threshold
cp Output/exposure* Output/collateral_threshold
cp Output/xva.csv Output/collateral_threshold
cp Output/colva* Output/collateral_threshold

# run collateral postprocessor
echo ""
echo "3) Run ORE to postprocess the NPV cube, with collateral (threshold=0)"
echo ""
$exe Input/ore_mta.xml

mkdir Output/collateral_mta
cp Output/log.txt Output/collateral_mta
cp Output/exposure* Output/collateral_mta
cp Output/xva.csv Output/collateral_mta
cp Output/colva* Output/collateral_mta

# run collateral postprocessor
echo ""
echo "4) Run ORE to postprocess the NPV cube, with collateral (threshold=mta=0)"
echo ""
$exe Input/ore_mpor.xml

mkdir Output/collateral_mpor
cp Output/log.txt Output/collateral_mpor
cp Output/exposure* Output/collateral_mpor
cp Output/xva.csv Output/collateral_mpor
cp Output/colva* Output/collateral_mpor

# run collateral postprocessor
echo ""
echo "5) Run ORE to postprocess the NPV cube, with collateral (threshold>0), exercise next break"
echo ""
$exe Input/ore_threshold_break.xml

mkdir Output/collateral_threshold_break
cp Output/log.txt Output/collateral_threshold_break
cp Output/exposure* Output/collateral_threshold_break
cp Output/xva.csv Output/collateral_threshold_break
cp Output/colva* Output/collateral_threshold_break

# plot
echo ""
echo "6) Plot results"
echo ""
cd Output
gnuplot plot.gp

echo ""
echo "View Output/plot*.pdf"
echo ""

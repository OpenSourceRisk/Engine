#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposure: Credit                                 |")
print("+--------------------------------------------------+")

# Legacy example 33

oreex.run("Input/ore_credit.xml")

oreex.run("Input/ore_creditoptions.xml")

# This is a bit hacky - get NPVs from the npv report and timeToExpiry from the additional results
# because maturity in the NPV report is fixed at the underlying maturity

npvreport = open("Output/credit/npv.csv")
npv = []
npvCol = 4
for line in npvreport.readlines():
    if "CreditDefaultSwapOption" in line:
        npv.append(float(line.split(',')[npvCol]))
#print (npv)

results = open("Output/credit/additional_results.csv")
time = []
valueCol = 3
for line in results.readlines():
    if "exerciseTime" in line:
        time.append(float(line.split(',')[valueCol]))
#print (time)

oreex.setup_plot("exposure_credit")
oreex.plot("credit/exposure_trade_CDS.csv", 2, 3, 'b', "CDS EPE")
oreex.plot("credit/exposure_trade_CDS.csv", 2, 4, 'r', "CDS ENE")
oreex.plot_line_marker(time, npv, 'g', "CDS Options NPV", marker='s')
oreex.decorate_plot(title="CDS Exposure vs Analytical CDS Option Prices")
oreex.save_plot_to_file()

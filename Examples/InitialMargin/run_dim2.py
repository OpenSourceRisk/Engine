#!/usr/bin/env python

import glob
import os
import sys
import pandas as pd
import lxml.etree as etree
import utilities
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Dynamic SIMM                                        |")
print("+-----------------------------------------------------+")

oreex.print_headline("Run ORE to generate the npv cube without swaption vol simulation")
oreex.run("Input/Dim2/ore_cube.xml")

oreex.print_headline("Run ORE to generate a scenario dump only, with swaption vol simulation")
oreex.run("Input/Dim2/ore_scenarios.xml")

oreex.print_headline("Convert scenario file into a market data file, one per scenario covering all dates")
refDates = utilities.scenarioToMarket('Input/Dim2/simulation.xml', 'Output/Dim2/scenariodump.csv', 'Input/DimValidation')

filterSample = 1

oreex.print_headline("Extract NPV evolution from the raw cube for sample " + str(filterSample))
cubeFile = 'Output/Dim2/rawcube.csv'
cubeData = utilities.rawCubeFilter(cubeFile, filterSample)

oreex.print_headline("Convert aggregation scenario data file into a fixing file for sample " + str(filterSample))
numeraireData = utilities.scenarioToFixings("Output/Dim2/scenariodata.csv.gz", refDates, filterSample, "Input/DimValidation")

orexml = 'Input/DimValidation/ore-1.xml'
doc = etree.parse(orexml)
columns = ["#TradeId", "asofDate", "NPV(Base)"] 
rowlist = []

for asof in refDates:
    nodes = doc.xpath('//ORE/Setup/Parameter[@name="asofDate"]')
    nodes[0].text = asof
    doc.write(orexml)

    print(orexml, "updated with asofDate", nodes[0].text)
    
    oreex.print_headline("Run ORE on the implied market as of " + asof)
    oreex.run(orexml)

    npvData = pd.read_csv('Output/DimValidation/npv.csv')
    
    for index, row in npvData.iterrows():
        trade = row['#TradeId']
        npv = float(row['NPV(Base)'])
        rowlist.append([trade, asof, '{:.6f}'.format(npv)])

npvData = pd.DataFrame(rowlist, columns=columns)

print("Undiscounted NPVs from ORE runs using implied markets")
print(npvData)

print("Numeraire data:")
print(numeraireData)

print("Discounted NPVs from the raw cube:")
print(cubeData)

comparisonData = utilities.npvComparison(npvData, cubeData, numeraireData);
print(comparisonData)

resultFile = "Output/DimValidation/npv_comparison.csv"
comparisonData.to_csv(resultFile, sep=',')

oreex.setup_plot("npv_comparison")
#oreex.plot("DimValidation/npv_comparison.csv", 0, 3, 'b', "Undiscounted NPVs")
oreex.plot("DimValidation/npv_comparison.csv", 0, 4, 'b', "Discounted NPVs")
oreex.plot("DimValidation/npv_comparison.csv", 0, 5, 'r', "Raw Cube Discounted NPVs")
oreex.decorate_plot(title="NPV Validation", ylabel="NPV", xlabel="Time Steps")
oreex.save_plot_to_file()

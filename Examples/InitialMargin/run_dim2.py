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

filterSample = 1
nettingSet = "CPTY_A"

oreex.print_headline("Run ORE to generate the npv cube and scenario dump, with swaption vol simulation")
oreex.run("Input/Dim2/ore.xml")

oreex.print_headline("Convert scenario " + str(filterSample) + " into a market data file")
refDates = utilities.scenarioToMarket('Input/Dim2/simulation.xml', 'Output/Dim2/scenariodump.csv', filterSample, 'Input/DimValidation')

oreex.print_headline("Extract NPV evolution from the raw cube for scenario " + str(filterSample))
#cubeFile = 'Output/Dim2/rawcube.csv'
#cubeData = utilities.rawCubeFilter(cubeFile, filterSample)
cubeFile = 'Output/Dim2/netcube.csv'
cubeData = utilities.netCubeFilter(cubeFile, nettingSet, filterSample)

oreex.print_headline("Convert aggregation scenario data file into a fixing file for sample " + str(filterSample))
numeraireData = utilities.scenarioToFixings("Output/Dim2/scenariodata.csv.gz", refDates, filterSample, "Input/DimValidation")

orecrif = 'Input/DimValidation/ore_crif.xml'
doccrif = etree.parse(orecrif)

oresimm = 'Input/DimValidation/ore_simm.xml'
docsimm = etree.parse(oresimm)

npvColumns = ["NettingSet", "asofDate", "NPV(Base)"] 
npvRowlist = []

simmColumns = ["Portfolio","AsOfDate", "SimmSide","InitialMargin","Currency"]
simmRowList = []

npvFile = "Output/DimValidation/npv.csv"
simmFile = "Output/DimValidation/simm.csv"
crifFile = "Output/DimValidation/crif.csv"

for asof in refDates:

    # delete output files
    if os.path.isfile(npvFile):
        os.remove(npvFile)
    if os.path.isfile(simmFile):
        os.remove(simmFile)
    if os.path.isfile(crifFile):
        os.remove(crifFile)

    # update asof in ore_crif.xml and run ORE
    nodes = doccrif.xpath('//ORE/Setup/Parameter[@name="asofDate"]')
    nodes[0].text = asof
    doccrif.write(orecrif)
    print()
    print(orecrif, "updated with asofDate", nodes[0].text)
    oreex.print_headline("Run ORE for CRIF on the implied market as of " + asof)
    oreex.run(orecrif)

    # update asof in ore_simm.xml and run ORE
    nodes = docsimm.xpath('//ORE/Setup/Parameter[@name="asofDate"]')
    nodes[0].text = asof
    docsimm.write(oresimm)
    print()
    print(oresimm, "updated with asofDate", nodes[0].text)
    oreex.print_headline("Run ORE for SIMM on the implied market as of " + asof)
    oreex.run(oresimm)

    # aggregate NPVs across the selected netting set and append to the npv row list
    value = 0
    if os.path.isfile(npvFile):
        npvData = pd.read_csv(npvFile)
        for index, row in npvData.iterrows():
            nid = row['NettingSet']
            npv = float(row['NPV(Base)'])
            if nid == nettingSet:
                value = value + npv
        npvRowlist.append([nid, asof, '{:.6f}'.format(value)])

    # pick total SIMM to Call for the selected netting set and append to the simm row list
    if os.path.isfile(simmFile):
        simmData = pd.read_csv(simmFile)
        for index, row in simmData.iterrows():
            portfolio = row['#Portfolio']
            productClass = row['ProductClass']
            riskClass = row['RiskClass']
            marginType = row['MarginType']
            bucket = row['Bucket']
            simmSide = row['SimmSide']
            regulation = row['Regulation']
            initialMargin = float(row['InitialMargin'])
            currency = row['Currency']
            if portfolio == nettingSet and productClass == 'All' and riskClass == 'All' and marginType == 'All' and bucket == 'All' and simmSide == 'Call':
                simmRowList.append([portfolio, asof, simmSide, '{:.6f}'.format(initialMargin), currency])
                break
        
npvData = pd.DataFrame(npvRowlist, columns=npvColumns)

print("Undiscounted NPVs from ORE runs using implied markets")
print(npvData)

print("Numeraire data:")
print(numeraireData)

print("Discounted NPVs from the raw cube:")
print(cubeData)

# These data frames should match, i.e. one line each per asof
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

simmData = pd.DataFrame(simmRowList, columns=simmColumns)
print(simmData)

simmResultFile = "Output/DimValidation/simm_evolution.csv"
simmData.to_csv(simmResultFile, sep=',')

oreex.setup_plot("simm_evolution")
oreex.plot("DimValidation/simm_evolution.csv", 0, 4, 'b', "SIMM")
oreex.decorate_plot(title="SIMM Evolution", ylabel="SIMM", xlabel="Time Steps")
oreex.save_plot_to_file()

oreex.print_headline("Run ORE for Dynamic SIMM with AMC/CG")
oreex.run("Input/Dim2/ore_amccg.xml")

oreex.setup_plot("dim_comparison")
oreex.plot("DimValidation/simm_evolution.csv", 0, 4, 'b', "SIMM")
oreex.plot("Dim2/AmcCg/dim_evolution.csv", 0, 4, 'r', "Dynamic SIMM with AMC/CG")
oreex.decorate_plot(title="DIM Evolution", ylabel="IM", xlabel="Time Steps")
oreex.save_plot_to_file()

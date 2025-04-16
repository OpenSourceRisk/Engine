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

# We select a single sample if filterSample > 0, otherwise process all available samples 
filterSample = 0
nettingSet = "CPTY_A"

# Number of samples specified in the simulation.xml
samples = utilities.getSamples('Input/Dim2/simulation.xml')
sampleRange = list(range(1, samples + 1))
if filterSample > 0:
    sampleRange = [filterSample]
print("samples in simulation.xml:", samples)
print("sample range:", sampleRange)

# Simulation to generate NPV cube and scenario dump
oreex.print_headline("Run ORE to generate the npv cube and scenario dump, with swaption vol simulation")
oreex.run("Input/Dim2/ore.xml")

simmFile = "Output/DimValidation/simm.csv"
simmCubeFile = "Output/DimValidation/simm_cube.csv"
simmColumns = ["Portfolio","AsOfDate", "SimmSide","InitialMargin","Currency","Sample"]

oresimm = 'Input/DimValidation/ore_simm.xml'
docsimm = etree.parse(oresimm)

if os.path.isfile(simmCubeFile):
    os.remove(simmCubeFile)

for s in sampleRange:
    print ("pick sample:", s)

    oreex.print_headline("Convert scenario " + str(s) + " into a market data file")
    refDates = utilities.scenarioToMarket('Input/Dim2/simulation.xml', 'Output/Dim2/scenariodump.csv', s, 'Input/DimValidation')

    oreex.print_headline("Convert aggregation scenario data file into a fixing file for sample " + str(s))
    numeraireData = utilities.scenarioToFixings("Output/Dim2/scenariodata.csv.gz", refDates, s, "Input/DimValidation")

    simmResultFile = "Output/DimValidation/simm_evolution_" + str(s) + ".csv"
    simmRowList = []

    for asof in refDates:

        # delete output file
        if os.path.isfile(simmFile):
            os.remove(simmFile)

        # update asof in ore_simm.xml and run ORE
        nodes = docsimm.xpath('//ORE/Setup/Parameter[@name="asofDate"]')
        nodes[0].text = asof
        docsimm.write(oresimm)
        print()
        print(oresimm, "updated with asofDate", nodes[0].text)
        oreex.print_headline("Run ORE for SIMM on the implied market as of " + asof)
        oreex.run(oresimm)

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
                initialMargin = float(row['InitialMargin'])
                currency = row['Currency']
                if portfolio == nettingSet and productClass == 'All' and riskClass == 'All' and marginType == 'All' and bucket == 'All' and simmSide == 'Call':
                    simmRowList.append([portfolio, asof, simmSide, '{:.6f}'.format(initialMargin), currency, s])
                    break
        
            simmData = pd.DataFrame(simmRowList, columns=simmColumns)
            #print(simmData)

            simmData.to_csv(simmResultFile, sep=',')

            simmData.to_csv(simmCubeFile, sep=',', mode='a', header=not os.path.exists(simmCubeFile))
            
        
oreex.setup_plot("simm_evolution")
oreex.plot("DimValidation/simm_evolution_1.csv", 0, 4, 'b', "Sample 1")
oreex.plot("DimValidation/simm_evolution_2.csv", 0, 4, 'r', "Sample 2")
oreex.plot("DimValidation/simm_evolution_3.csv", 0, 4, 'g', "Sample 3")
oreex.decorate_plot(title="SIMM Evolution", ylabel="SIMM", xlabel="Time Steps")
oreex.save_plot_to_file()

#oreex.print_headline("Run ORE for Dynamic SIMM with AMC/CG")
#oreex.run("Input/Dim2/ore_amccg.xml")

#oreex.setup_plot("dim_comparison")
#oreex.plot("DimValidation/simm_evolution.csv", 0, 4, 'b', "SIMM")
#oreex.plot("Dim2/AmcCg/dim_evolution.csv", 0, 4, 'r', "Dynamic SIMM with AMC/CG")
#oreex.decorate_plot(title="DIM Evolution", ylabel="IM", xlabel="Time Steps")
#oreex.save_plot_to_file()

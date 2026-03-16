#!/usr/bin/env python

# The purpose of this script is to produce a SIMM cube file, i.e. numeraire-discounted SIMM
# (full, delta, vega, curvature margin)
# for the portfolio defined in Input/Dim2/portfolio.xml,
# for the date grid and number of samples defined in Input/Dim2/simulation.xml.
#
# This is done by simulating the IR/FX market as specified in Input/Dim2/simulation.xml,
# dumping the market scenarios for all dates and samples to Output/Dim2/scenariodump.csv,
# converting the scenariodump for each path into a market data and fixing file in ORE format
# running the usual SIMM analytic in ORE for each date on the path
# parsing the SIMM report, concatenating to a row list that is finally turned into a pandas
# dataframe and saved to csv.

# The purpose of this "brute force" SIMM cube is to validate the Dynamic SIMM results,
# see Input/ore_amccg.xml, produced for the same portfolio and date grid, and written to
# Output/Dim2/AmcCg/simm_cube.csv.

# Separate scripts then use these SIMM cubes to compare
# - SIMM evolutions (expected, and pathwise for a few selcted paths)
# - SIMM distributions at selected dates

# Note:
# 1) For the SIMM evolution benchmarking it is essential that brute-force and AMCCG
#    calculation use the same simulation configuration (Euler, same time steps per year, same date grid),
#    with different numnbers of samples (a few for the brute-force, many such as 10k for AMCCG
#    to achieve good quality regressions, even if we only want to compare a few resulting paths).
# 2) For the SIMM distribution benchmarking we can run brute-force with a different simulation
#    config than AMCCG, i.e. just a few selected dates with Exact discretization, but reasonably large number
#    samples (e.g. 1000); we can reuse the same AMCCG cube as for the evolution benchmarking

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

# Specify the netting set to pick, just in case there are several in the portfolio
nettingSet = "CPTY_A"

asof0 = utilities.getAsOfDate("Input/Dim2/ore.xml")
print("AsOfDate:", asof0)

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

# Run ORE for t0 SIMM
oreex.print_headline("Run ORE to generate the t0 SIMM report")
oreex.run("Input/Dim2/ore_simm.xml")

oreex.print_headline("Extract NPV evolution from the raw cube for scenario " + str(filterSample))
cubeFile = 'Output/Dim2/netcube.csv'
cubeData = utilities.netCubeFilter(cubeFile, nettingSet, filterSample, asof0)

npvColumns = ["NettingSet", "asofDate", "NPV(Base)"] 
npvRowlist = []

t0SimmFile = "Output/Dim2/simm.csv"
npvFile = "Output/DimValidation/npv.csv"
simmFile = "Output/DimValidation/simm.csv"
simmCubeFile = "Output/DimValidation/simm_cube.csv"
simmColumns = ["Portfolio","Sample","AsOfDate","Time","InitialMargin","Currency","SimmSide","Depth","MarginType"]

oresimm = 'Input/DimValidation/ore_simm.xml'
docsimm = etree.parse(oresimm)

depth_dict = { "All": 0, "Delta": 1, "Vega": 2, "Curvature": 3 }

if os.path.isfile(simmCubeFile):
    os.remove(simmCubeFile)

for s in sampleRange:
    print ("pick sample:", s)

    oreex.print_headline("Convert scenario " + str(s) + " into a market data file")
    refDates = utilities.scenarioToMarket('Input/Dim2/simulation.xml', 'Output/Dim2/scenariodump.csv', s, 'Input/DimValidation')

    oreex.print_headline("Convert aggregation scenario data file into a fixing file for sample " + str(s))
    numeraireData = utilities.scenarioToFixings("Output/Dim2/scenariodata.csv.gz", refDates, s, "Input/DimValidation")

    simmRowList = []

    # add t0 SIMM to the row list
    if os.path.isfile(t0SimmFile):
        simmData = pd.read_csv(t0SimmFile)
        for index, row in simmData.iterrows():
            pf = row['#Portfolio']
            pc = row['ProductClass']
            rc = row['RiskClass']
            mt = row['MarginType']
            bk = row['Bucket']
            side = row['SimmSide']
            im = float(row['InitialMargin']) 
            currency = row['Currency']
            depth = depth_dict[mt]
            if pf == nettingSet and pc == 'All' and rc == 'All' and bk == 'All' and side == 'Call':
                simmRowList.append([pf, s, asof0, 0.0, '{:.6f}'.format(im), currency, side, depth, mt])
                
    for asof in refDates:

        numeraire = utilities.num(asof, numeraireData)

        time = utilities.getTimeDifference(asof0, asof) 

        # delete output file
        if os.path.isfile(simmFile):
            os.remove(simmFile)

        # update asof in ore_simm.xml and run ORE
        nodes = docsimm.xpath('//ORE/Setup/Parameter[@name="asofDate"]')
        nodes[0].text = asof
        docsimm.write(oresimm)
        print()
        print(oresimm, "updated with asofDate", nodes[0].text)
        oreex.print_headline("Run ORE for SIMM on the implied market as of " + asof + " for sample " + str(s))
        oreex.run(oresimm)

        # pick SIMM to Call for the selected netting set and append to the simm row list
        if os.path.isfile(simmFile):
            simmData = pd.read_csv(simmFile)
            for index, row in simmData.iterrows():
                pf = row['#Portfolio']
                pc = row['ProductClass']
                rc = row['RiskClass']
                mt = row['MarginType']
                bk = row['Bucket']
                side = row['SimmSide']
                im = float(row['InitialMargin']) / numeraire
                currency = row['Currency']
                depth = depth_dict[mt]
                if pf == nettingSet and pc == 'All' and rc == 'All' and bk == 'All' and side == 'Call':
                    simmRowList.append([pf, s, asof, '{:.4f}'.format(time), '{:.6f}'.format(im), currency, side, depth, mt])
                
        # aggregate NPVs across the selected netting set and append to the simm row list, depth 4
        value = 0
        if os.path.isfile(npvFile):
            npvData = pd.read_csv(npvFile)
            for index, row in npvData.iterrows():
                nid = row['NettingSet']
                npv = float(row['NPV(Base)'])
                if nid == nettingSet:
                    value = value + npv
        simmRowList.append([nettingSet, s, asof, '{:.4f}'.format(time), '{:.6f}'.format(value/numeraire), currency, "", 4, "NPV"])

    simmData = pd.DataFrame(simmRowList, columns=simmColumns)

    simmData.to_csv(simmCubeFile, sep=',', mode='a', header=not os.path.exists(simmCubeFile), index=False)

print("done")

#simmAvgFile = "Output/DimValidation/simm_evolution_avg.csv"
#utilities.expectedSimmEvolution(simmCubeFile, simmAvgFile)

import os
import sys
import subprocess
import pandas as pd
import lxml.etree as etree
import utilities
import argparse
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(False)

parser = argparse.ArgumentParser("Dynamic SIMM Benchmarking")
parser.add_argument("--portfolio", nargs="?", help="portfolio xml file", type=str)
parser.add_argument("--pathwise", default=False, action="store_true", help="pathwise benchmarking if true, distribution otherwise")
args = parser.parse_args()

print("portfolio:", args.portfolio)
print("pathwise: ", args.pathwise)

if args.portfolio == "portfolio_swaption.xml" or args.portfolio == "portfolio_bermudan.xml":
    path_grid = "121,1M"
    dist_grid = "1M,1Y,2Y,3Y,4Y,5Y,10Y1M"
else:
    path_grid = "65,1M"
    dist_grid = "1M,1Y,2Y,3Y,4Y,5Y,5Y1M"

path_samples_benchmark = "10"
dist_samples_benchmark_1 = "1000"
dist_samples_benchmark_2 = "10000"
samples_dynamicsimm = "10000"
# a bit slow, unfortuantely, but we need 10k for a non-spiky plot
#samples_regression = "10000"
samples_regression = "1000"

def updatePortfolio(oreFile, prefix = ""):
    doc = etree.parse(oreFile)
    nodes = doc.xpath('//ORE/Setup/Parameter[@name="portfolioFile"]')
    nodes[0].text = prefix + args.portfolio
    doc.write(oreFile)
    print(oreFile, "updated with portfolio file", args.portfolio)
    
def updateSimulation(simulationFile, grid, samples):
    doc = etree.parse(simulationFile)
    nodes1 = doc.xpath('//Simulation/Parameters/Grid')
    nodes1[0].text = grid
    nodes2 = doc.xpath('//Simulation/Parameters/Samples')
    nodes2[0].text = samples
    doc.write(simulationFile)
    print(simulationFile, "updated with grid", grid, "and samples", samples )

def updateCollateral(simmReport, collateralXml):
    simm = "0"
    if os.path.isfile(simmReport):
        simmData = pd.read_csv(simmReport)
        for index, row in simmData.iterrows():
            pc = row['ProductClass']
            rc = row['RiskClass']
            mt = row['MarginType']
            bk = row['Bucket']
            side = row['SimmSide']
            im = row['InitialMargin']
            if pc == 'All' and rc == 'All' and mt == 'All' and bk == 'All' and side == 'Call':
                simm = im

        doc = etree.parse(collateralXml)
        nodes = doc.xpath('//CollateralBalances/CollateralBalance/InitialMargin')
        nodes[0].text = str(simm)
        doc.write(collateralXml)
        print(collateralXml, "updated with t0 SIMM", simm)
    else:
        print("SIMM report does not exist:", simmReport)

    
ore = 'Input/Dim2/ore.xml'
ore_dynamicsimm = 'Input/Dim2/ore_dynamicsimm.xml'
ore_benchmark = 'Input/Dim2/ore_benchmark.xml'
ore_regression = 'Input/Dim2/ore_regression.xml'
ore_regression_unscaled = 'Input/Dim2/ore_regression_unscaled.xml'
ore_t0simm = 'Input/Dim2/ore_simm.xml'
ore_simm = 'Input/DimValidation/ore_simm.xml'

simulation = 'Input/Dim2/simulation.xml'
simulation_dynamicsimm = 'Input/Dim2/simulation_amccg.xml'
simulation_benchmark = 'Input/Dim2/simulation_benchmark.xml'
simulation_regression = 'Input/Dim2/simulation_regression.xml'

###########################################################
# update all relevant ore files with the selected portfolio 
###########################################################

updatePortfolio(ore)
updatePortfolio(ore_dynamicsimm)
updatePortfolio(ore_benchmark)
updatePortfolio(ore_regression)
updatePortfolio(ore_regression_unscaled)
updatePortfolio(ore_simm, "../Dim2/")
updatePortfolio(ore_t0simm)

##############################################################
# update simulation grid and samples
# - for pathwise benchmarking on a "fine" grid
# - for distribution benchmarking on a coarse grid
##############################################################

if args.pathwise:
    # a few samples only
    updateSimulation(simulation, path_grid, path_samples_benchmark)
    updateSimulation(simulation_benchmark, path_grid, path_samples_benchmark)
    # large number of samples, because we need to regress across many to get a few paths right
    updateSimulation(simulation_dynamicsimm , path_grid, samples_dynamicsimm)
    updateSimulation(simulation_regression , path_grid, samples_regression)
else:
    # coarse grid with relatively large number of samples
    updateSimulation(simulation, dist_grid, dist_samples_benchmark_1)
    updateSimulation(simulation_benchmark, dist_grid, dist_samples_benchmark_2)
    updateSimulation(simulation_dynamicsimm , dist_grid, samples_dynamicsimm)
    updateSimulation(simulation_regression , dist_grid, samples_regression)
    
###################################################
# run ore processes for t0 SIMM, Dynamic SIMM,
# benchmark II and simple regression
###################################################

oreex.print_headline("Run ORE for t0 SIMM")
oreex.run(ore_t0simm)

oreex.print_headline("Run ORE for Dynamic SIMM")
oreex.run(ore_dynamicsimm)

oreex.print_headline("Run ORE for Benchmark II")
oreex.run(ore_benchmark)

if args.portfolio != "portfolio_bermudan.xml":
    # this needs to go first, because the scaled version below reuses the cube
    oreex.print_headline("Run ORE for unscaled simple regression")
    oreex.run(ore_regression_unscaled)

    oreex.print_headline("Run ORE for scaled simple regression")
    # update collateral for t0 scaling
    updateCollateral('Output/Dim2/simm.csv', 'Input/Dim2/collateralbalances.xml')
    oreex.run(ore_regression)

####################################################
# run the SIMM cube for Benchmark I, time consuming,
# only for pathwise benchmarking
####################################################

if args.pathwise:
    subprocess.call("python3 run_simmcube.py", shell=True)

########################################################
# call plot utilities manually specific to portfolio and
# purpose (pathwise or distribution)
########################################################

# See e.g.
# plot_dim_evolution_fxoption.py
# plot_dim_distribution_fxoption.py

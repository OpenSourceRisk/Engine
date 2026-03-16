#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample
import pca
import os

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposure: Hull-White 2f                          |")
print("+--------------------------------------------------+")

# remove override
if "OVERWRITE_SCENARIOGENERATOR_SAMPLES" in os.environ.keys() :
    samples1=os.environ["OVERWRITE_SCENARIOGENERATOR_SAMPLES"]
    os.environ["OVERWRITE_SCENARIOGENERATOR_SAMPLES"]=""

# Legacy example 37
oreex.print_headline("Run ORE to produce scenariodump.csv")
oreex.run("Input/ore_hw2f_calibration.xml")

oreex.print_headline("Run python script to perform PCA on scenariodump data")
pca.run_pca('hw2f_calibration')

oreex.print_headline("Plot results: Eigenvectors")

oreex.setup_plot("hw2f_eigenvectors")
oreex.plot("hw2f_calibration/eigenvectors.csv", 0, 1, 'b', "Model Eigenvector 1")
oreex.plot("hw2f_calibration/eigenvectors.csv", 0, 2, 'r', "Model Eigenvector 2")
oreex.plot("../Input/inputeigenvectors_hw2f.csv", 0, 1, 'y', "Input Eigenvector 1")
oreex.plot("../Input/inputeigenvectors_hw2f.csv", 0, 2, 'g', "Input Eigenvector 2")
oreex.decorate_plot(title="Model implied principal components vs. input principal components", ylabel="shift",
                    y_format_as_int = False, display_grid = True, legend_loc = "best")
oreex.save_plot_to_file()

# same as before, but with calibration to coterminal swaptions
oreex.print_headline("Run ORE to produce scenariodump.csv (risk neutral volatility)")
oreex.run("Input/ore_hw2f_calibration_rn.xml")

oreex.print_headline("Run python script to perform PCA on scenariodump data (risk neutral volatility)")
pca.run_pca('hw2f_calibration_rn')

oreex.print_headline("Plot results: Eigenvectors")

oreex.setup_plot("hw2f_eigenvectors_rn")
oreex.plot("hw2f_calibration_rn/eigenvectors.csv", 0, 1, 'b', "Model Eigenvector 1")
oreex.plot("hw2f_calibration_rn/eigenvectors.csv", 0, 2, 'r', "Model Eigenvector 2")
oreex.plot("../Input/inputeigenvectors_hw2f.csv", 0, 1, 'y', "Input Eigenvector 1")
oreex.plot("../Input/inputeigenvectors_hw2f.csv", 0, 2, 'g', "Input Eigenvector 2")
oreex.decorate_plot(title="Model implied principal components vs. input principal components", ylabel="shift",
                    y_format_as_int = False, display_grid = True, legend_loc = "best")
oreex.save_plot_to_file()

# restore override
if "OVERWRITE_SCENARIOGENERATOR_SAMPLES" in os.environ.keys() :
    os.environ["OVERWRITE_SCENARIOGENERATOR_SAMPLES"]=samples1

# Legacy example 38
oreex.print_headline("Run ORE to produce NPV cube and exposures for Cross Currency Swaps")
oreex.run("Input/ore_hw2f.xml")

oreex.print_headline("Plot Cross Currency Swap results")
oreex.setup_plot("exposure_hw2f_ccswap")
oreex.plot("hw2f/exposure_trade_CCSwap.csv", 2, 3, 'b', "EPE CCSwap")
oreex.plot("hw2f/exposure_trade_CCSwap.csv", 2, 4, 'r', "ENE CCSwap")
oreex.decorate_plot(title="Exposures under HW2F")
oreex.save_plot_to_file()

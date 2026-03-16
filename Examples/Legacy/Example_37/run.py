#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

import pca
import os
        
oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce scenariodump.csv")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Run python script to perform PCA on scenariodump data")

pca.run_pca()

oreex.print_headline("Plot results: Eigenvectors")

oreex.setup_plot("Eigenvectors")
oreex.plot("eigenvectors.csv", 0, 1, 'b', "Model Eigenvector 1")
oreex.plot("eigenvectors.csv", 0, 2, 'r', "Model Eigenvector 2")
oreex.plot("../Input/inputeigenvectors.csv", 0, 1, 'y', "Input Eigenvector 1")
oreex.plot("../Input/inputeigenvectors.csv", 0, 2, 'g', "Input Eigenvector 2")
# oreex.plot("exposure_trade_Swap_20y.csv", 2, 4, 'r', "Swap ENE")
# # oreex.plot_npv("swaption_npv.csv", 6, 'g', "NPV Swaptions", marker='s')
oreex.decorate_plot(title="Example 37 - Model implied principal components vs. input principal components", ylabel="shift",
                    y_format_as_int = False, display_grid = True, legend_loc = "best")
oreex.save_plot_to_file()


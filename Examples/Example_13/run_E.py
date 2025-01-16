#!/usr/bin/env python

import glob

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

import sys
oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE: Case E, FX Option, zero order regression")
oreex.run("Input/ore_E0.xml")
oreex.get_times("Output/log_E0.txt")
oreex.save_output_to_subdir(
    "case_E_fxopt",
    ["log_E0.txt", "dim_evolution_E0.csv", "dim_regression_E0.csv"]
)

oreex.print_headline("Run ORE: case E, FX Option, first order regression")
oreex.run("Input/ore_E1.xml")
oreex.get_times("Output/log_E1.txt")
oreex.save_output_to_subdir(
    "case_E_fxopt",
    ["log_E1.txt", "dim_evolution_E1.csv", "dim_regression_E1.csv"]
)

oreex.print_headline("Run ORE: Case E, FX Option, second order regression")
oreex.run("Input/ore_E2.xml")
oreex.get_times("Output/log_E2.txt")
oreex.save_output_to_subdir(
    "case_E_fxopt",
    ["log_E2.txt", "dim_evolution_E2.csv", "dim_regression_E2.csv"]
)

oreex.print_headline("Run ORE: Case E, FX Option, Dynamic Delta VaR")
oreex.run("Input/ore_E3.xml")
oreex.get_times("Output/log_E3.txt")
oreex.save_output_to_subdir(
    "case_E_fxopt",
    ["log_E3.txt", "dim_evolution_E3.csv", "dim_regression_E3.csv"]
)

oreex.print_headline("Plot results")

oreex.setup_plot("dim_evolution_E_fxopt")
oreex.plot(os.path.join("case_E_fxopt", "dim_evolution_E0.csv"), 0, 6, 'y', "Simple")
oreex.plot(os.path.join("case_E_fxopt", "dim_evolution_E0.csv"), 0, 4, 'c', "Zero Order Regression")
oreex.plot(os.path.join("case_E_fxopt", "dim_evolution_E1.csv"), 0, 4, 'm', "First Order Regression")
oreex.plot(os.path.join("case_E_fxopt", "dim_evolution_E2.csv"), 0, 4, 'r', "Second Order Regression")
oreex.plot(os.path.join("case_E_fxopt", "dim_evolution_E3.csv"), 0, 3, 'b', "Dynamic Delta VaR")
oreex.decorate_plot(title="Example 13 (E) - DIM Evolution FX Option EUR/USD", xlabel="Timestep", ylabel="DIM", legend_loc="lower left")
oreex.save_plot_to_file()

# TODO: Extend the DIM related postprocessor output so that we can avoid scaling and squaring while plotting
#oreex.setup_plot("dim_regression_A_swap_eur")
#oreex.plotScaled(os.path.join("case_A_eur_swap", "dim_regression_1.csv"), 1, 6, 'c', 'Simulation Data', marker='+', linestyle='', exponent=2.0, yScale=1e10, xScale=1, zoom=7, rescale=True)
#oreex.plotScaled(os.path.join("case_A_eur_swap", "dim_regression_0.csv"), 1, 2, 'm', 'Zero Order Regression', exponent=2.0, yScale=2.33*2.33*1e10, xScale=1)
#oreex.plotScaled(os.path.join("case_A_eur_swap", "dim_regression_1.csv"), 1, 2, 'g', 'First Order Regression', exponent=2.0, yScale=2.33*2.33*1e10, xScale=1)
#oreex.plotScaled(os.path.join("case_A_eur_swap", "dim_regression_2.csv"), 1, 2, 'b', 'Second Order Regression', exponent=2.0, yScale=2.33*2.33*1e10, xScale=1, legendLocation='upper right')
#oreex.decorate_plot(title="Example 13 (A) - DIM Regression Swap EUR, Timestep 100", xlabel='Rate', ylabel='Clean NPV Variance')
#oreex.save_plot_to_file()


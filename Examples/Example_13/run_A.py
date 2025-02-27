#!/usr/bin/env python

import glob

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

import sys
oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE (case A (swap eur), 1st order regression)")
oreex.run("Input/ore_A1.xml")
oreex.get_times("Output/log_1.txt")
oreex.save_output_to_subdir(
    "case_A_eur_swap",
    ["log_1.txt", "dim_evolution_1.csv", "dim_regression_1.csv"]
)
oreex.print_headline("Run ORE (case A (swap eur), 2nd order regression)")
oreex.run("Input/ore_A2.xml")
oreex.save_output_to_subdir(
    "case_A_eur_swap",
    ["log_2.txt", "dim_evolution_2.csv", "dim_regression_2.csv"]
)
oreex.print_headline("Run ORE (case A (swap eur), zero order regression)")
oreex.run("Input/ore_A0.xml")
oreex.save_output_to_subdir(
    "case_A_eur_swap",
    ["log_0.txt", "dim_evolution_0.csv", "dim_regression_0.csv"]
)
oreex.print_headline("Run ORE (case A (swap eur), flat extrapolation of t0 IM)")
oreex.run("Input/ore_A4.xml")
oreex.save_output_to_subdir(
    "case_A_eur_swap",
    ["log_4.txt", "dim_evolution_4.csv", "exposure_nettingset_CPTY_A.csv"]
)
oreex.print_headline("Run ORE (case A (swap eur), Dynamic Delta VaR)")
oreex.run("Input/ore_A5.xml")
oreex.save_output_to_subdir(
    "case_A_eur_swap",
    ["log_5.txt", "dim_evolution_5.csv", "exposure_nettingset_CPTY_A.csv"]
)

oreex.print_headline("Plot results")

oreex.setup_plot("dim_evolution_A_swap_eur")
oreex.plot(os.path.join("case_A_eur_swap", "dim_evolution_0.csv"), 0, 6, 'y', "Simple")
oreex.plot(os.path.join("case_A_eur_swap", "dim_evolution_0.csv"), 0, 4, 'c', "Zero Order Regression")
oreex.plot(os.path.join("case_A_eur_swap", "dim_evolution_1.csv"), 0, 4, 'm', "First Order Regression")
oreex.plot(os.path.join("case_A_eur_swap", "dim_evolution_2.csv"), 0, 4, 'r', "Second Order Regression")
oreex.plot(os.path.join("case_A_eur_swap", "dim_evolution_5.csv"), 0, 3, 'b', "Dynamic Delta VaR")
oreex.decorate_plot(title="Example 13 (A) - DIM Evolution Swap EUR", xlabel="Timestep", ylabel="DIM")
oreex.save_plot_to_file()

# TODO: Extend the DIM related postprocessor output so that we can avoid scaling and squaring while plotting
oreex.setup_plot("dim_regression_A_swap_eur")
oreex.plotScaled(os.path.join("case_A_eur_swap", "dim_regression_1.csv"), 1, 6, 'c', 'Simulation Data', marker='+', linestyle='', exponent=2.0, yScale=1e10, xScale=1, zoom=7, rescale=True)
oreex.plotScaled(os.path.join("case_A_eur_swap", "dim_regression_0.csv"), 1, 2, 'm', 'Zero Order Regression', exponent=2.0, yScale=2.33*2.33*1e10, xScale=1)
oreex.plotScaled(os.path.join("case_A_eur_swap", "dim_regression_1.csv"), 1, 2, 'g', 'First Order Regression', exponent=2.0, yScale=2.33*2.33*1e10, xScale=1)
oreex.plotScaled(os.path.join("case_A_eur_swap", "dim_regression_2.csv"), 1, 2, 'b', 'Second Order Regression', exponent=2.0, yScale=2.33*2.33*1e10, xScale=1, legendLocation='upper right')
oreex.decorate_plot(title="Example 13 (A) - DIM Regression Swap EUR, Timestep 100", xlabel='Rate', ylabel='Clean NPV Variance')
oreex.save_plot_to_file()


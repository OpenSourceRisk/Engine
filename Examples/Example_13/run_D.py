#!/usr/bin/env python

import glob

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

import sys
oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

# Case D
oreex.print_headline("Run ORE (case D (swap eur-usd), 1st order regression)")
oreex.run("Input/ore_D1.xml")
oreex.get_times("Output/log_1.txt")
oreex.save_output_to_subdir(
    "case_D_eurusd_swap",
    ["log_1.txt", "dim_evolution_1.txt", "dim_regression_1.txt"]
)
oreex.print_headline("Run ORE (case D (swap eur-usd), 2nd order regression)")
oreex.run("Input/ore_D2.xml")
oreex.save_output_to_subdir(
    "case_D_eurusd_swap",
    ["log_2.txt", "dim_evolution_2.txt", "dim_regression_2.txt"]
)

oreex.print_headline("Plot results")

oreex.setup_plot("dim_evolution_D_swap_eurusd")
oreex.plot(os.path.join("case_D_eurusd_swap", "dim_evolution_1.txt"), 0, 3, 'y', "Zero Order Regression")
oreex.plot(os.path.join("case_D_eurusd_swap", "dim_evolution_1.txt"), 0, 4, 'c', "First Order Regression")
oreex.plot(os.path.join("case_D_eurusd_swap", "dim_evolution_2.txt"), 0, 4, 'm', "Second Order Regression")
oreex.decorate_plot(title="Example 13 (D) - DIM Evolution Swap EUR-USD", xlabel="Timestep", ylabel="DIM")
oreex.save_plot_to_file()

# plot the regression ? how ?

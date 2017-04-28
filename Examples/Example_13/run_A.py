#!/usr/bin/env python

import glob

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

import sys
oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

# Case A
oreex.print_headline("Run ORE (case A (swap eur), 1st order regression)")
oreex.run("Input/ore_A1.xml")
oreex.get_times("Output/log_1.txt")
oreex.save_output_to_subdir(
    "case_A_eur_swap",
    ["log_1.txt", "dim_evolution_1.txt", "dim_regression_1.txt"]
)
oreex.print_headline("Run ORE (case A (swap eur), 2nd order regression)")
oreex.run("Input/ore_A2.xml")
oreex.save_output_to_subdir(
    "case_A_eur_swap",
    ["log_2.txt", "dim_evolution_2.txt", "dim_regression_2.txt"]
)
oreex.print_headline("Run ORE (case A (swap eur), 2nd order (NPV) regression)")
oreex.run("Input/ore_A3.xml")
oreex.save_output_to_subdir(
    "case_A_eur_swap",
    ["log_3.txt", "dim_evolution_3.txt", "dim_regression_3.txt"]
)

oreex.print_headline("Plot results")

oreex.setup_plot("dim_evolution_A_swap_eur")
oreex.plot(os.path.join("case_A_eur_swap", "dim_evolution_1.txt"), 0, 3, 'y', "Zero Order Regression")
oreex.plot(os.path.join("case_A_eur_swap", "dim_evolution_1.txt"), 0, 4, 'c', "First Order Regression")
oreex.plot(os.path.join("case_A_eur_swap", "dim_evolution_2.txt"), 0, 4, 'm', "Second Order Regression")
oreex.plot(os.path.join("case_A_eur_swap", "dim_evolution_3.txt"), 0, 4, 'b', "Second Order NPV Regression")
oreex.plot(os.path.join("case_A_eur_swap", "dim_evolution_2.txt"), 0, 6, 'r', "Simple DIM")
oreex.decorate_plot(title="Example 13 (A) - DIM Evolution Swap EUR", xlabel="Timestep", ylabel="DIM")
oreex.save_plot_to_file()

oreex.setup_plot("dim_regression_A_swap_eur")
oreex.plot(os.path.join("case_A_eur_swap", "dim_regression_1.txt"), 1, 6, 'k', "Delta NPV", marker='+', linestyle='')
oreex.plot(os.path.join("case_A_eur_swap", "dim_regression_1.txt"), 1, 5, 'y', "Zero Order Regression")
oreex.plot(os.path.join("case_A_eur_swap", "dim_regression_1.txt"), 1, 2, 'c', "First Order Regression")
oreex.plot(os.path.join("case_A_eur_swap", "dim_regression_2.txt"), 1, 2, 'm', "Second Order Regression")
oreex.plot(os.path.join("case_A_eur_swap", "dim_regression_2.txt"), 1, 7, 'r', "Simple DIM")
oreex.decorate_plot(title="Example 13 (A) - DIM Regression Swap EUR, Timestep 100", xlabel="Regressor", ylabel="DIM")
oreex.save_plot_to_file()


#!/usr/bin/env python

import glob

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

import sys
oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

# Case B
oreex.print_headline("Run ORE (case B (swaption eur), 1st order regression, t=300)")
oreex.run("Input/ore_B1.xml")
oreex.get_times("Output/log_1.txt")
oreex.save_output_to_subdir(
    "case_B_eur_swaption",
    ["log_1.txt", "dim_evolution_1.txt", "dim_regression_1.txt"]
)
oreex.print_headline("Run ORE (case B (swaption eur), 2nd order regression, t=300)")
oreex.run("Input/ore_B2.xml")
oreex.save_output_to_subdir(
    "case_B_eur_swaption",
    ["log_2.txt", "dim_evolution_2.txt", "dim_regression_2.txt"]
)

oreex.print_headline("Run ORE (case B (swaption eur), 1st order regression, t=100)")
oreex.run("Input/ore_B1b.xml")
oreex.save_output_to_subdir(
    "case_B_eur_swaption",
    ["log_1b.txt", "dim_evolution_1b.txt", "dim_regression_1b.txt"]
)
oreex.print_headline("Run ORE (case B (swaption eur), 2nd order regression, t=100)")
oreex.run("Input/ore_B2b.xml")
oreex.save_output_to_subdir(
    "case_B_eur_swaption",
    ["log_2b.txt", "dim_evolution_2b.txt", "dim_regression_2b.txt"]
)

oreex.print_headline("Plot results")

oreex.setup_plot("dim_evolution_B_swaption_eur")
oreex.plot(os.path.join("case_B_eur_swaption", "dim_evolution_1.txt"), 0, 3, 'y', "Zero Order Regression")
oreex.plot(os.path.join("case_B_eur_swaption", "dim_evolution_1.txt"), 0, 4, 'c', "First Order Regression")
oreex.plot(os.path.join("case_B_eur_swaption", "dim_evolution_2.txt"), 0, 4, 'm', "Second Order Regression")
oreex.decorate_plot(title="Example 13 (B) - DIM Evolution Swaption (physical delivery) EUR", xlabel="Timestep", ylabel="DIM")
oreex.save_plot_to_file()

oreex.setup_plot("dim_regression_B_swaption_eur_t300")
oreex.plotSq(os.path.join("case_B_eur_swaption", "dim_regression_1.txt"), 1, 6, 'k', "Delta NPV", marker='+', linestyle='')
oreex.plotSq(os.path.join("case_B_eur_swaption", "dim_regression_1.txt"), 1, 5, 'y', "Zero Order Regression", marker='.', linestyle='')
oreex.plotSq(os.path.join("case_B_eur_swaption", "dim_regression_1.txt"), 1, 2, 'c', "First Order Regression", marker='.', linestyle='')
oreex.plotSq(os.path.join("case_B_eur_swaption", "dim_regression_2.txt"), 1, 2, 'm', "Second Order Regression", marker='.', linestyle='', title="Example 13 (B) - DIM Regression Swaption (physical delivery) EUR, Timestep 300", xlabel="Regressor", ylabel="Variance", rescale = True, zoom=2)
oreex.save_plot_to_file()

oreex.setup_plot("dim_regression_B_swaption_eur_t100")
oreex.plotSq(os.path.join("case_B_eur_swaption", "dim_regression_1b.txt"), 1, 6, 'k', "Delta NPV", marker='+', linestyle='')
oreex.plotSq(os.path.join("case_B_eur_swaption", "dim_regression_1b.txt"), 1, 5, 'y', "Zero Order Regression", marker='', linestyle='-')
oreex.plotSq(os.path.join("case_B_eur_swaption", "dim_regression_1b.txt"), 1, 2, 'c', "First Order Regression", marker='', linestyle='-')
oreex.plotSq(os.path.join("case_B_eur_swaption", "dim_regression_2b.txt"), 1, 2, 'm', "Second Order Regression", marker='', linestyle='-', title="Example 13 (B) - DIM Regression Swaption (physical delivery) EUR, Timestep 100", xlabel="Regressor", ylabel="Variance", rescale = True, zoom=2)
oreex.save_plot_to_file()


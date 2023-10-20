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
oreex.print_headline("Run ORE (case B (swaption eur), zero order regression, t=300)")
oreex.run("Input/ore_B0.xml")
oreex.save_output_to_subdir(
    "case_B_eur_swaption",
    ["log_0.txt", "dim_evolution_0.txt", "dim_regression_0.txt"]
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
oreex.print_headline("Run ORE (case B (swaption eur), zero order regression, t=100)")
oreex.run("Input/ore_B0b.xml")
oreex.save_output_to_subdir(
    "case_B_eur_swaption",
    ["log_0b.txt", "dim_evolution_0b.txt", "dim_regression_0b.txt"]
)

oreex.print_headline("Plot results")

oreex.setup_plot("dim_evolution_B_swaption_eur")
oreex.plot(os.path.join("case_B_eur_swaption", "dim_evolution_1.txt"), 0, 3, 'y', "Zero Order Regression")
oreex.plot(os.path.join("case_B_eur_swaption", "dim_evolution_1.txt"), 0, 4, 'c', "First Order Regression")
oreex.plot(os.path.join("case_B_eur_swaption", "dim_evolution_2.txt"), 0, 4, 'm', "Second Order Regression")
oreex.decorate_plot(title="Example 13 (B) - DIM Evolution Swaption (physical delivery) EUR", xlabel="Timestep", ylabel="DIM")
oreex.save_plot_to_file()

oreex.setup_plot("dim_regression_B_swaption_eur_t300")
oreex.plotScaled(os.path.join("case_B_eur_swaption", "dim_regression_1.txt"), 1, 6, 'c', 'Simulation Data', marker='+', linestyle='', exponent=2.0, yScale=1e9, xScale=1, zoom=7, rescale=True)
oreex.plotScaled(os.path.join("case_B_eur_swaption", "dim_regression_0.txt"), 1, 2, 'm', 'Zero Order Regression', exponent=2.0, yScale=2.33*2.33*1e9, xScale=1)
oreex.plotScaled(os.path.join("case_B_eur_swaption", "dim_regression_1.txt"), 1, 2, 'g', 'First Order Regression', exponent=2.0, yScale=2.33*2.33*1e9, xScale=1)
oreex.plotScaled(os.path.join("case_B_eur_swaption", "dim_regression_2.txt"), 1, 2, 'b', 'Second Order Regression', exponent=2.0, yScale=2.33*2.33*1e9, xScale=1, legendLocation='upper right')
oreex.decorate_plot( title="Example 13 (B) - DIM Regression Swaption (physical delivery) EUR, Timestep 300", xlabel="Regressor", ylabel="Clean NPV Variance")
oreex.save_plot_to_file()

oreex.setup_plot("dim_regression_B_swaption_eur_t100")
oreex.plotScaled(os.path.join("case_B_eur_swaption", "dim_regression_1b.txt"), 1, 6, 'c', 'Simulation Data', marker='+', linestyle='', exponent=2.0, yScale=1e9, xScale=1, zoom=7, rescale=True)
oreex.plotScaled(os.path.join("case_B_eur_swaption", "dim_regression_0b.txt"), 1, 2, 'm', 'Zero Order Regression', exponent=2.0, yScale=2.33*2.33*1e9, xScale=1)
oreex.plotScaled(os.path.join("case_B_eur_swaption", "dim_regression_1b.txt"), 1, 2, 'g', 'First Order Regression', exponent=2.0, yScale=2.33*2.33*1e9, xScale=1)
oreex.plotScaled(os.path.join("case_B_eur_swaption", "dim_regression_2b.txt"), 1, 2, 'b', 'Second Order Regression', exponent=2.0, yScale=2.33*2.33*1e9, xScale=1, legendLocation='upper right')
oreex.decorate_plot( title="Example 13 (B) - DIM Regression Swaption (physical delivery) EUR, Timestep 100", xlabel="Regressor", ylabel="Clean NPV Variance")
oreex.save_plot_to_file()


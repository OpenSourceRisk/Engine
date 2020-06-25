#!/usr/bin/env python

import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

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
oreex.print_headline("Run ORE (case A (swap eur), zero order regression)")
oreex.run("Input/ore_A0.xml")
oreex.save_output_to_subdir(
    "case_A_eur_swap",
    ["log_0.txt", "dim_evolution_0.txt", "dim_regression_0.txt"]
)

oreex.print_headline("Plot results")

oreex.setup_plot("dim_evolution_A_swap_eur")
oreex.plot(os.path.join("case_A_eur_swap", "dim_evolution_1.txt"), 0, 3, 'y', "Zero Order Regression")
oreex.plot(os.path.join("case_A_eur_swap", "dim_evolution_1.txt"), 0, 4, 'c', "First Order Regression")
oreex.plot(os.path.join("case_A_eur_swap", "dim_evolution_2.txt"), 0, 4, 'm', "Second Order Regression")
#oreex.plot(os.path.join("case_A_eur_swap", "dim_evolution_2.txt"), 0, 6, 'r', "Simple DIM")
oreex.decorate_plot(title="Example 13 (A) - DIM Evolution Swap EUR", xlabel="Timestep", ylabel="DIM")
oreex.save_plot_to_file()

# TODO: Extend the DIM related postprocessor output so that we can avoid scaling and squaring while plotting
oreex.setup_plot("dim_regression_A_swap_eur")
oreex.plotScaled(os.path.join("case_A_eur_swap", "dim_regression_1.txt"), 1, 6, 'c', 'Simulation Data', marker='+', linestyle='', exponent=2.0, yScale=1e10, xScale=1, zoom=7, rescale=True)
oreex.plotScaled(os.path.join("case_A_eur_swap", "dim_regression_0.txt"), 1, 2, 'm', 'Zero Order Regression', exponent=2.0, yScale=2.33*2.33*1e10, xScale=1)
oreex.plotScaled(os.path.join("case_A_eur_swap", "dim_regression_1.txt"), 1, 2, 'g', 'First Order Regression', exponent=2.0, yScale=2.33*2.33*1e10, xScale=1)
oreex.plotScaled(os.path.join("case_A_eur_swap", "dim_regression_2.txt"), 1, 2, 'b', 'Second Order Regression', exponent=2.0, yScale=2.33*2.33*1e10, xScale=1, legendLocation='upper right')
oreex.decorate_plot(title="Example 13 (A) - DIM Regression Swap EUR, Timestep 100", xlabel='Rate', ylabel='Clean NPV Variance')
oreex.save_plot_to_file()

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

# Case C
oreex.print_headline("Run ORE (case C (swap usd), 1st order regression)")
oreex.run("Input/ore_C1.xml")
oreex.get_times("Output/log_1.txt")
oreex.save_output_to_subdir(
    "case_C_usd_swap",
    ["log_1.txt", "dim_evolution_1.txt", "dim_regression_1.txt"]
)
oreex.print_headline("Run ORE (case C (swap usd), 2nd order regression)")
oreex.run("Input/ore_C2.xml")
oreex.save_output_to_subdir(
    "case_C_usd_swap",
    ["log_2.txt", "dim_evolution_2.txt", "dim_regression_2.txt"]
)

oreex.print_headline("Plot results")

oreex.setup_plot("dim_evolution_C_swap_usd")
oreex.plot(os.path.join("case_C_usd_swap", "dim_evolution_1.txt"), 0, 3, 'y', "Zero Order Regression")
oreex.plot(os.path.join("case_C_usd_swap", "dim_evolution_1.txt"), 0, 4, 'c', "First Order Regression")
oreex.plot(os.path.join("case_C_usd_swap", "dim_evolution_2.txt"), 0, 4, 'm', "Second Order Regression")
oreex.decorate_plot(title="Example 13 (C) - DIM Evolution Swap USD", xlabel="Timestep", ylabel="DIM")
oreex.save_plot_to_file()

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

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
oreex.plot(os.path.join("case_B_eur_swaption", "dim_regression_1.txt"), 1, 6, 'k', "Delta NPV", marker='+', linestyle='')
oreex.plot(os.path.join("case_B_eur_swaption", "dim_regression_1.txt"), 1, 5, 'y', "Zero Order Regression", marker='.', linestyle='')
oreex.plot(os.path.join("case_B_eur_swaption", "dim_regression_1.txt"), 1, 2, 'c', "First Order Regression", marker='.', linestyle='')
oreex.plot(os.path.join("case_B_eur_swaption", "dim_regression_2.txt"), 1, 2, 'm', "Second Order Regression", marker='.', linestyle='')
oreex.decorate_plot( title="Example 13 (B) - DIM Regression Swaption (physical delivery) EUR, Timestep 300", xlabel="Regressor", ylabel="DIM")
oreex.save_plot_to_file()

oreex.setup_plot("dim_regression_B_swaption_eur_t100")
oreex.plot(os.path.join("case_B_eur_swaption", "dim_regression_1b.txt"), 1, 6, 'k', "Delta NPV", marker='+', linestyle='')
oreex.plot(os.path.join("case_B_eur_swaption", "dim_regression_1b.txt"), 1, 5, 'y', "Zero Order Regression", marker='.', linestyle='')
oreex.plot(os.path.join("case_B_eur_swaption", "dim_regression_1b.txt"), 1, 2, 'c', "First Order Regression", marker='.', linestyle='')
oreex.plot(os.path.join("case_B_eur_swaption", "dim_regression_2b.txt"), 1, 2, 'm', "Second Order Regression", marker='.', linestyle='')
oreex.decorate_plot(title="Example 13 (B) - DIM Regression Swaption (physical delivery) EUR, Timestep 100", xlabel="Regressor", ylabel="DIM")
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

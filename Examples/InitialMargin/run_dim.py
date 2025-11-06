#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Dynamic IM and MVA                                  |")
print("+-----------------------------------------------------+")

# Legacy example 13

# Case A: Single Currency Swap in Domestic Currency

oreex.print_headline("Run ORE (case A (swap eur), zero order regression)")
oreex.run("Input/Dim/ore_A0.xml")
oreex.print_headline("Run ORE (case A (swap eur), 1st order regression)")
oreex.run("Input/Dim/ore_A1.xml")
oreex.print_headline("Run ORE (case A (swap eur), 2nd order regression)")
oreex.run("Input/Dim/ore_A2.xml")
oreex.print_headline("Run ORE (case A (swap eur), flat extrapolation of t0 IM)")
oreex.run("Input/Dim/ore_A4.xml")
oreex.print_headline("Run ORE (case A (swap eur), Dynamic Delta VaR)")
oreex.run("Input/Dim/ore_A5.xml")

oreex.print_headline("Plot results")

oreex.setup_plot("dim_evolution_A_swap_eur")
oreex.plot("dim/A0/dim_evolution.csv", 0, 6, 'y', "Simple")
oreex.plot("dim/A0/dim_evolution.csv", 0, 4, 'y', "Zero Order Regression")
oreex.plot("dim/A1/dim_evolution.csv", 0, 4, 'c', "First Order Regression")
oreex.plot("dim/A2/dim_evolution.csv", 0, 4, 'm', "Second Order Regression")
oreex.decorate_plot(title="DIM Evolution Swap EUR", xlabel="Timestep", ylabel="DIM")
oreex.save_plot_to_file()

oreex.setup_plot("dim_evolution_A_swap_eur_ddv")
oreex.plot("dim/A1/dim_evolution.csv", 0, 6, 'y', "Simple")
oreex.plot("dim/A1/dim_evolution.csv", 0, 4, 'r', "Regression")
oreex.plot("dim/A5/dim_evolution.csv", 0, 3, 'b', "Dynamic Delta VaR")
oreex.decorate_plot(title="DIM Evolution Swap EUR", xlabel="Timestep", ylabel="DIM")
oreex.save_plot_to_file()

# TODO: Extend the DIM related postprocessor output so that we can avoid scaling and squaring while plotting
oreex.setup_plot("dim_regression_A_swap_eur")
oreex.plotScaled("dim/A1/dim_regression.csv", 1, 6, 'c', 'Simulation Data', marker='+', linestyle='', exponent=2.0, yScale=1e10, xScale=1, zoom=7, rescale=True)
oreex.plotScaled("dim/A0/dim_regression.csv", 1, 2, 'm', 'Zero Order Regression', exponent=2.0, yScale=2.33*2.33*1e10, xScale=1)
oreex.plotScaled("dim/A1/dim_regression.csv", 1, 2, 'g', 'First Order Regression', exponent=2.0, yScale=2.33*2.33*1e10, xScale=1)
oreex.plotScaled("dim/A2/dim_regression.csv", 1, 2, 'b', 'Second Order Regression', exponent=2.0, yScale=2.33*2.33*1e10, xScale=1, legendLocation='upper right')
oreex.decorate_plot(title="DIM Regression Swap EUR, Timestep 100", xlabel='Rate', ylabel='Clean NPV Variance')
oreex.save_plot_to_file()

# Case B: European Swaption

oreex.print_headline("Run ORE (case B (swaption eur), zero order regression, t=300)")
oreex.run("Input/Dim/ore_B0.xml")
oreex.print_headline("Run ORE (case B (swaption eur), zero order regression, t=100)")
oreex.run("Input/Dim/ore_B0b.xml")
oreex.print_headline("Run ORE (case B (swaption eur), 1st order regression, t=300)")
oreex.run("Input/Dim/ore_B1.xml")
oreex.print_headline("Run ORE (case B (swaption eur), 1st order regression, t=100)")
oreex.run("Input/Dim/ore_B1b.xml")
oreex.print_headline("Run ORE (case B (swaption eur), 2nd order regression, t=300)")
oreex.run("Input/Dim/ore_B2.xml")
oreex.print_headline("Run ORE (case B (swaption eur), 2nd order regression, t=100)")
oreex.run("Input/Dim/ore_B2b.xml")
oreex.print_headline("Run ORE (case B (swaption eur), Dynamic Delta VaR)")
oreex.run("Input/Dim/ore_B3.xml")

oreex.print_headline("Plot results")

oreex.setup_plot("dim_evolution_B_swaption_eur")
oreex.plot("dim/B1/dim_evolution.csv", 0, 3, 'y', "Zero Order Regression")
oreex.plot("dim/B1/dim_evolution.csv", 0, 4, 'c', "First Order Regression")
oreex.plot("dim/B2/dim_evolution.csv", 0, 4, 'm', "Second Order Regression")
oreex.plot("dim/B3/dim_evolution.csv", 0, 3, 'b', "Dynamic Delta VaR")
oreex.decorate_plot(title="DIM Evolution Swaption (physical delivery) EUR", xlabel="Timestep", ylabel="DIM")
oreex.save_plot_to_file()

oreex.setup_plot("dim_regression_B_swaption_eur_t300")
oreex.plotScaled("dim/B1/dim_regression.csv", 1, 6, 'c', 'Simulation Data', marker='+', linestyle='', exponent=2.0, yScale=1e9, xScale=1, zoom=7, rescale=True)
oreex.plotScaled("dim/B0/dim_regression.csv", 1, 2, 'm', 'Zero Order Regression', exponent=2.0, yScale=2.33*2.33*1e9, xScale=1)
oreex.plotScaled("dim/B1/dim_regression.csv", 1, 2, 'g', 'First Order Regression', exponent=2.0, yScale=2.33*2.33*1e9, xScale=1)
oreex.plotScaled("dim/B2/dim_regression.csv", 1, 2, 'b', 'Second Order Regression', exponent=2.0, yScale=2.33*2.33*1e9, xScale=1, legendLocation='upper right')
oreex.decorate_plot(title="DIM Regression Swaption (physical delivery) EUR, Timestep 300", xlabel="Regressor", ylabel="Clean NPV Variance")
oreex.save_plot_to_file()

oreex.setup_plot("dim_regression_B_swaption_eur_t100")
oreex.plotScaled("dim/B1b/dim_regression.csv", 1, 6, 'c', 'Simulation Data', marker='+', linestyle='', exponent=2.0, yScale=1e9, xScale=1, zoom=7, rescale=True)
oreex.plotScaled("dim/B0b/dim_regression.csv", 1, 2, 'm', 'Zero Order Regression', exponent=2.0, yScale=2.33*2.33*1e9, xScale=1)
oreex.plotScaled("dim/B1b/dim_regression.csv", 1, 2, 'g', 'First Order Regression', exponent=2.0, yScale=2.33*2.33*1e9, xScale=1)
oreex.plotScaled("dim/B2b/dim_regression.csv", 1, 2, 'b', 'Second Order Regression', exponent=2.0, yScale=2.33*2.33*1e9, xScale=1, legendLocation='upper right')
oreex.decorate_plot(title="DIM Regression Swaption (physical delivery) EUR, Timestep 100", xlabel="Regressor", ylabel="Clean NPV Variance")
oreex.save_plot_to_file()

# Case C: Single Currency Swap in Foreign Currency

oreex.print_headline("Run ORE (case C (swap usd), 1st order regression)")
oreex.run("Input/Dim/ore_C1.xml")
oreex.print_headline("Run ORE (case C (swap usd), 2nd order regression)")
oreex.run("Input/Dim/ore_C2.xml")

oreex.print_headline("Plot results")

oreex.setup_plot("dim_evolution_C_swap_usd")
oreex.plot("dim/C1/dim_evolution.csv", 0, 3, 'y', "Zero Order Regression")
oreex.plot("dim/C1/dim_evolution.csv", 0, 4, 'c', "First Order Regression")
oreex.plot("dim/C2/dim_evolution.csv", 0, 4, 'm', "Second Order Regression")
oreex.decorate_plot(title="DIM Evolution Swap USD", xlabel="Timestep", ylabel="DIM")
oreex.save_plot_to_file()

# Case D: Cross Currency Swap

oreex.print_headline("Run ORE (case D (swap eur-usd), 1st order regression)")
oreex.run("Input/Dim/ore_D1.xml")
oreex.print_headline("Run ORE (case D (swap eur-usd), 2nd order regression)")
oreex.run("Input/Dim/ore_D2.xml")

oreex.print_headline("Plot results")

oreex.setup_plot("dim_evolution_D_swap_eurusd")
oreex.plot("dim/D1/dim_evolution.csv", 0, 3, 'y', "Zero Order Regression")
oreex.plot("dim/D1/dim_evolution.csv", 0, 4, 'c', "First Order Regression")
oreex.plot("dim/D2/dim_evolution.csv", 0, 4, 'm', "Second Order Regression")
oreex.decorate_plot(title="DIM Evolution Swap EUR-USD", xlabel="Timestep", ylabel="DIM")
oreex.save_plot_to_file()

# Case E: FX Option

oreex.print_headline("Run ORE: Case FX Option, zero order regression")
oreex.run("Input/Dim/ore_E0.xml")
oreex.print_headline("Run ORE: case FX Option, first order regression")
oreex.run("Input/Dim/ore_E1.xml")
oreex.print_headline("Run ORE: Case FX Option, second order regression")
oreex.run("Input/Dim/ore_E2.xml")
oreex.print_headline("Run ORE: Case FX Option, Dynamic Delta VaR")
oreex.run("Input/Dim/ore_E3.xml")

oreex.print_headline("Plot results")

oreex.setup_plot("dim_evolution_E_fxopt")
oreex.plot("dim/E0/dim_evolution.csv", 0, 6, 'y', "Simple")
oreex.plot("dim/E0/dim_evolution.csv", 0, 4, 'c', "Zero Order Regression")
oreex.plot("dim/E1/dim_evolution.csv", 0, 4, 'm', "First Order Regression")
oreex.plot("dim/E2/dim_evolution.csv", 0, 4, 'r', "Second Order Regression")
oreex.plot("dim/E3/dim_evolution.csv", 0, 3, 'b', "Dynamic Delta VaR")
oreex.decorate_plot(title="DIM Evolution FX Option EUR/USD", xlabel="Timestep", ylabel="DIM", legend_loc="lower left")
oreex.save_plot_to_file()



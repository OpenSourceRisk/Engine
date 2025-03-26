#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Collateral: Impacts on Exposure with Close-Out Grid |")
print("+-----------------------------------------------------+")

# Legacy example 31

# coarse date grid
oreex.print_headline("Run simulation, uncollateralised exposure")
oreex.run("Input/ore_closeout.xml")

# MPoR effect 
oreex.print_headline("Re-run with collateral, threshold=mta=0")
oreex.run("Input/ore_closeout_mpor.xml")

# monthly date grid
oreex.print_headline("Run simulation, exposure with collateral, threshold=mta=0 and regression IM")
oreex.run("Input/ore_closeout_dim.xml")

# monthly date grid and storage of simulated sensitivities for dynamic Delta VaR IM 
oreex.print_headline("Run simulation, exposure with collateral, threshold=mta=0 and delta VaR IM")
oreex.run("Input/ore_closeout_ddv.xml")

oreex.print_headline("Plot results")

oreex.setup_plot("closeout_nocollateral_epe")
oreex.plot("closeout_nocollateral/exposure_trade_Swap_1.csv", 2, 3, 'b', "EPE Swap 1")
oreex.plot("closeout_nocollateral/exposure_trade_Swap_2.csv", 2, 3, 'r', "EPE Swap 2")
oreex.plot("closeout_nocollateral/exposure_trade_Swap_3.csv", 2, 3, 'g', "EPE Swap 3")
oreex.plot("closeout_nocollateral/exposure_nettingset_CPTY_A.csv", 2, 3, 'm', "EPE NettingSet")
oreex.decorate_plot(title="Effect of Collateral, Close-Out Grid")
oreex.save_plot_to_file()

oreex.setup_plot("closeout_nocollateral_ene")
oreex.plot("closeout_nocollateral/exposure_trade_Swap_1.csv", 2, 4, 'b', "ENE Swap 1")
oreex.plot("closeout_nocollateral/exposure_trade_Swap_2.csv", 2, 4, 'r', "ENE Swap 2")
oreex.plot("closeout_nocollateral/exposure_trade_Swap_3.csv", 2, 4, 'g', "ENE Swap 3")
oreex.plot("closeout_nocollateral/exposure_nettingset_CPTY_A.csv", 2, 4, 'm', "ENE NettingSet")
oreex.decorate_plot(title="Effect of Collateral, Close-Out Grid")
oreex.save_plot_to_file()

oreex.setup_plot("closeout_mpor_epe")
oreex.plot("closeout_nocollateral/exposure_nettingset_CPTY_A.csv", 2, 3, 'b', "EPE NettingSet")
oreex.plot("closeout_mpor/exposure_nettingset_CPTY_A.csv", 2, 3, 'r', "EPE NettingSet, MPOR 2W")
oreex.decorate_plot(title="Effect of Collateral, Close-Out Grid")
oreex.save_plot_to_file()

oreex.setup_plot("closeout_dim_epe")
oreex.plot("closeout_mpor/exposure_nettingset_CPTY_A.csv", 2, 3, 'r', "EPE NettingSet, MPOR 2W")
oreex.plot("closeout_dim/exposure_nettingset_CPTY_A.csv", 2, 3, 'g', "EPE NettingSet, MPOR 2W and Regression DIM")
oreex.decorate_plot(title="Effect of Collateral, Close-Out Grid")
oreex.save_plot_to_file()

oreex.setup_plot("closeout_dim_evolution")
oreex.plot("closeout_dim/dim_evolution.csv", 0, 4, 'c', "Regression")
oreex.plot("closeout_ddv/dim_evolution.csv", 0, 3, 'm', "Delta VaR")
oreex.decorate_plot(title="Effect of Collateral, DIM Evolution", xlabel="Timestep", ylabel="DIM", legend_loc="lower left")
oreex.save_plot_to_file()


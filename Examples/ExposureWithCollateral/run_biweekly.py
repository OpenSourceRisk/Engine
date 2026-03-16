#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+------------------------------------------------------------------------------+")
print("| Collateral: IAH, Threshold, MTA, MPOR and Initial Margin Effects on Exposure |")
print("+------------------------------------------------------------------------------+")

# Legacy Example 10

oreex.print_headline("Run simulation, uncollateralised exposure")
oreex.run("Input/ore.xml")

oreex.print_headline("Re-run with collateral and threshold > 0")
oreex.run("Input/ore_threshold.xml")

oreex.print_headline("Re-run with collateral, threshold = 0, mta > 0")
oreex.run("Input/ore_mta.xml")

oreex.print_headline("Re-run with collateral, threshold = mta = 0")
oreex.run("Input/ore_mpor.xml")

oreex.print_headline("Re-run with collateral, threshold > 0, exercise next break")
oreex.run("Input/ore_threshold_break.xml")

oreex.print_headline("Re-run with collateral, threshold > 0 and dynamic initial margin")
oreex.run("Input/ore_threshold_dim.xml")

oreex.print_headline("Run IAH = 0")
oreex.run("Input/ore_iah_0.xml")

oreex.print_headline("Run IAH = 1m")
oreex.run("Input/ore_iah_1.xml")

oreex.print_headline("Plot results")

oreex.setup_plot("nocollateral_epe")
oreex.plot("nocollateral/exposure_trade_Swap_1.csv", 2, 3, 'b', "EPE Swap 1")
oreex.plot("nocollateral/exposure_trade_Swap_2.csv", 2, 3, 'r', "EPE Swap 2")
oreex.plot("nocollateral/exposure_trade_Swap_3.csv", 2, 3, 'g', "EPE Swap 3")
oreex.plot("nocollateral/exposure_nettingset_CPTY_A.csv", 2, 3, 'm', "EPE NettingSet")
oreex.decorate_plot(title="Effect of Collateral")
oreex.save_plot_to_file()

oreex.setup_plot("nocollateral_ene")
oreex.plot("nocollateral/exposure_trade_Swap_1.csv", 2, 4, 'b', "ENE Swap 1")
oreex.plot("nocollateral/exposure_trade_Swap_2.csv", 2, 4, 'r', "ENE Swap 2")
oreex.plot("nocollateral/exposure_trade_Swap_3.csv", 2, 4, 'g', "ENE Swap 3")
oreex.plot("nocollateral/exposure_nettingset_CPTY_A.csv", 2, 4, 'm', "ENE NettingSet")
oreex.decorate_plot(title="Effect of Collateral")
oreex.save_plot_to_file()

oreex.setup_plot("nocollateral_allocated_epe")
oreex.plot("nocollateral/exposure_trade_Swap_1.csv", 2, 5, 'b', "Allocated EPE Swap 1")
oreex.plot("nocollateral/exposure_trade_Swap_2.csv", 2, 5, 'r', "Allocated EPE Swap 2")
oreex.plot("nocollateral/exposure_trade_Swap_3.csv", 2, 5, 'g', "Allocated EPE Swap 3")
oreex.decorate_plot(title="Effect of Collateral")
oreex.save_plot_to_file()

oreex.setup_plot("nocollateral_allocated_ene")
oreex.plot("nocollateral/exposure_trade_Swap_1.csv", 2, 6, 'b', "Allocated ENE Swap 1")
oreex.plot("nocollateral/exposure_trade_Swap_2.csv", 2, 6, 'r', "Allocated ENE Swap 2")
oreex.plot("nocollateral/exposure_trade_Swap_3.csv", 2, 6, 'g', "Allocated ENE Swap 3")
oreex.decorate_plot(title="Effect of Collateral")
oreex.save_plot_to_file()

oreex.setup_plot("threshold_epe")
oreex.plot("vm_threshold/exposure_nettingset_CPTY_A.csv", 2, 3, 'b', "EPE NettingSet")
oreex.plot("vm_threshold/exposure_nettingset_CPTY_A.csv", 2, 3, 'r', "EPE NettingSet, Threshold 1m")
oreex.decorate_plot(title="Effect of Collateral")
oreex.save_plot_to_file()

oreex.setup_plot("threshold_allocated_epe")
oreex.plot("vm_threshold/exposure_trade_Swap_1.csv", 2, 5, 'b', "Allocated EPE Swap 1")
oreex.plot("vm_threshold/exposure_trade_Swap_2.csv", 2, 5, 'r', "Allocated EPE Swap 2")
oreex.plot("vm_threshold/exposure_trade_Swap_3.csv", 2, 5, 'g', "Allocated EPE Swap 3")
oreex.decorate_plot(title="Effect of Collateral")
oreex.save_plot_to_file()

oreex.setup_plot("mta_epe")
oreex.plot("nocollateral/exposure_nettingset_CPTY_A.csv", 2, 3, 'b', "EPE NettingSet")
oreex.plot("vm_mta/exposure_nettingset_CPTY_A.csv", 2, 3, 'r', "EPE NettingSet, MTA 100k")
oreex.decorate_plot(title="Effect of Collateral")
oreex.save_plot_to_file()

oreex.setup_plot("mpor_epe")
oreex.plot("nocollateral/exposure_nettingset_CPTY_A.csv", 2, 3, 'b', "EPE NettingSet")
oreex.plot("vm_mpor/exposure_nettingset_CPTY_A.csv", 2, 3, 'r', "EPE NettingSet, MPOR 2W")
oreex.decorate_plot(title="Effect of Collateral")
oreex.save_plot_to_file()

oreex.setup_plot("vm_balance")
oreex.plot("vm_threshold/colva_nettingset_CPTY_A.csv", 2, 3, 'b', "VM Balance", 2)
oreex.decorate_plot(title="Effect of Collateral", ylabel="VM Balance")
oreex.save_plot_to_file()

oreex.setup_plot("colva")
oreex.plot("vm_threshold/colva_nettingset_CPTY_A.csv", 2, 4, 'b', "COLVA Increments", 2)
oreex.decorate_plot(title="Effect of Collateral", ylabel="COLVA")
oreex.save_plot_to_file()

oreex.setup_plot("collateral_floor")
oreex.plot("vm_threshold/colva_nettingset_CPTY_A.csv", 2, 6, 'b', "Collateral Floor Increments", 2)
oreex.decorate_plot(title="Effect of Collateral", ylabel="Collateral Floor Increments")
oreex.save_plot_to_file()

oreex.setup_plot("threshold_break_epe")
oreex.plot("vm_threshold/exposure_nettingset_CPTY_A.csv", 2, 3, 'b', "EPE NettingSet, Threshold 1m")
oreex.plot("vm_threshold_break/exposure_nettingset_CPTY_A.csv", 2, 3, 'r', "EPE NettingSet, Threshold 1m, Breaks")
oreex.decorate_plot(title="Effect of Collateral")
oreex.save_plot_to_file()

oreex.setup_plot("exposures_iah")
oreex.plot("iah_0/exposure_nettingset_CPTY_A.csv", 2, 3, 'b', "EPE IAH=0")
oreex.plot("iah_1/exposure_nettingset_CPTY_A.csv", 2, 3, 'g', "EPE IAH=1m")
oreex.plot("iah_0/exposure_nettingset_CPTY_A.csv", 2, 5, 'r', "PFE IAH=0")
oreex.plot("iah_1/exposure_nettingset_CPTY_A.csv", 2, 5, 'y', "PFE IAH=1m")
oreex.decorate_plot(title="IAH Example")
oreex.save_plot_to_file()


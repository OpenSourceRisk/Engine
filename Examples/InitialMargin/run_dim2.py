#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Dynamic SIMM                                        |")
print("+-----------------------------------------------------+")

oreex.print_headline("Run ORE for a Bermudan Swaption, AMC, VM only")
oreex.run("Input/Dim2/ore_vm.xml")

oreex.setup_plot("dynamic_simm_exposure_nocollateral")
oreex.plot("Dim2/Amc/exposure_trade_BermudanSwaptionCash.csv", 2, 3, 'r', "EPE")
oreex.decorate_plot(title="Exposure without Collateral", xlabel="Time/Years", ylabel="Exposure")
oreex.save_plot_to_file()

oreex.setup_plot("dynamic_simm_exposure_vm")
oreex.plot("Dim2/Amc/exposure_nettingset_CPTY_A.csv", 2, 3, 'r', "EPE")
oreex.decorate_plot(title="Exposure wit Collateral, VM only", xlabel="Time/Years", ylabel="Exposure")
oreex.save_plot_to_file()

oreex.print_headline("Run ORE for a Bermudan Swaption, AMC/CG, VM and IM")
oreex.run("Input/Dim2/ore_im.xml")

oreex.setup_plot("dynamic_simm_evolution")
oreex.plot("Dim2/AmcCg/dim_evolution.csv", 8, 4, 'r', "Dynamic SIMM")
oreex.decorate_plot(title="Expected Dynamic SIMM", xlabel="Time/Years", ylabel="SIMM")
oreex.save_plot_to_file()

oreex.setup_plot("dynamic_simm_exposure_im")
oreex.plot("Dim2/AmcCg/exposure_nettingset_CPTY_A.csv", 2, 3, 'r', "EPE")
oreex.decorate_plot(title="Exposure with Collateral, VM and IM", xlabel="Time/Years", ylabel="Exposure")
oreex.save_plot_to_file()


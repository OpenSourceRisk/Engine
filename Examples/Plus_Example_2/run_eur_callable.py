#!/usr/bin/env python

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

import sys
oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV cube and exposures with AMC, without collateral")
oreex.run("Input/ore_eur_callable_amc.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot AMC results")

oreex.setup_plot("example_callable_swap")
oreex.plot("exposure_trade_Swap.csv", 2, 3, '#a2142f', "EPE Swap")
oreex.plot("exposure_nettingset_CPTY_A.csv", 2, 3, '#77ac30', "EPE Callable swap AMC")
oreex.plot("exposure_trade_Swaption.csv", 2, 3, '#7e2f8e', "EPE Swaption AMC (Physical)")
oreex.plot("exposure_trade_ShortSwap.csv", 2, 3, '#0072bd', "EPE Short Swap")
#oreex.plot("exposure_nettingset_CPTY_B.csv", 2, 3, 'g', "EPE Short Swap")


oreex.print_headline("Run ORE to produce NPV cube and exposures with Grid, without collateral")
oreex.run("Input/ore_eur_callable_grid.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot Grid results")

oreex.plot("exposure_nettingset_CPTY_A.csv", 2, 3, '#d95319', "EPE Callable swap Grid")
oreex.plot("exposure_trade_Swaption.csv", 2, 3, '#cb4679', "EPE Swaption Grid(Physical)")


oreex.decorate_plot(title="EUR Callable Swap", ylabel="Exposure")
oreex.save_plot_to_file()


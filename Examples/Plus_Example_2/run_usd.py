#!/usr/bin/env python

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

import sys
oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV cube and exposures with AMC, without collateral")
oreex.run("Input/ore_usd_amc.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot AMC results")

oreex.setup_plot("example_bermudan_swaption_usd_10_10")
oreex.plot("exposure_trade_SwaptionCash.csv", 2, 3, '#a2142f', "EPE Swaption AMC (Cash)")
oreex.plot("exposure_trade_SwaptionPhysical.csv", 2, 3, '#77ac30', "EPE Swaption AMC (Physical)")
oreex.plot("exposure_trade_Swap.csv", 2, 3, '#7e2f8e', "EPE Forward Swap")

oreex.print_headline("Run ORE to produce NPV cube and exposures with Grid, without collateral")
oreex.run("Input/ore_usd_grid.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot Grid results")
oreex.plot("exposure_trade_SwaptionCash.csv", 2, 3, '#0072bd', "EPE Swaption Grid (Cash)")
oreex.plot("exposure_trade_SwaptionPhysical.csv", 2, 3, '#d95319', "EPE Swaption Grid (Physical)")

oreex.decorate_plot(title="USD Bermudan Swaption 10Yx10Y", ylabel="Exposure")
oreex.save_plot_to_file()


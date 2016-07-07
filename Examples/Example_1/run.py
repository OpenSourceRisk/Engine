#!/usr/bin/env python

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

import sys
oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV cube and exposures")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Run ORE again to price European Swaptions")
oreex.run("Input/ore_swaption.xml")

oreex.print_headline("Plot results: Simulated exposures vs analytical swaption prices")

oreex.setup_plot("swaptions")
oreex.plot("exposure_trade_Swap_20y.csv", 2, 3, 'b', "EPE")
oreex.plot("exposure_trade_Swap_20y.csv", 2, 4, 'r', "ENE")
oreex.plot("swaption_npv.csv", 3, 4, 'r', "NPV")
oreex.decorate_plot(title="Example 1 - Simulated exposures vs analytical swaption prices")
oreex.save_plot_to_file()


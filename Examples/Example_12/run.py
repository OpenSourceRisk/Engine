#!/usr/bin/env python

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

import sys
oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV cube and exposures, without collateral")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot results: Simulated exposures vs analytical swaption prices")

#oreex.setup_plot("example_swaption")
#oreex.plot("exposure_trade_10_Swap_EUR_USD.csv", 2, 3, 'b', "Swap")
#oreex.plot("exposure_trade_10_Swap_EUR_USD_RESET.csv", 2, 3, 'r', "Resettable Swap")
#oreex.decorate_plot(title="Example 9")
#oreex.save_plot_to_file()

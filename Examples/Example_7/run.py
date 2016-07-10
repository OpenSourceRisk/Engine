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

oreex.setup_plot("example_swap_cash_physical")
oreex.plot("exposure_trade_Swap.csv", 2, 3, 'b', "EPE Swap")
oreex.plot("exposure_trade_SwaptionCash.csv", 2, 3, 'r', "EPE Swaption Cash")
oreex.plot("exposure_trade_SwaptionPhysical.csv", 2, 3, 'g', "EPE Swaption Physical")
oreex.decorate_plot(title="Example 7")
oreex.save_plot_to_file()

#!/usr/bin/env python

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

oreex = OreExample()

oreex.print_headline("Run ORE to produce NPV cube and exposures, without collateral")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.setup_plot("example_ccswap")
oreex.plot("exposure_trade_CCSwap.csv", 2, 3, 'b', "EPE CCSwap")
oreex.plot("exposure_trade_CCSwap.csv", 2, 4, 'r', "ENE CCSwap")
oreex.decorate_plot(title="Example 8")
oreex.save_plot_to_file()

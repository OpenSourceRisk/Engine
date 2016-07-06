#!/usr/bin/env python

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

oreex = OreExample()

oreex.print_headline("Run ORE to produce NPV cube and exposures, without collateral")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.setup_plot("example_swaption")
oreex.plot("exposure_trade_SwaptionCash.csv", 2, 3, 'b', "EPE Swaption (Cash)")
oreex.plot("exposure_trade_SwaptionPhysical.csv", 2, 3, 'r', "EPE Swaption (Physical)")
oreex.plot("exposure_trade_Swap.csv", 2, 3, 'g', "EPE Forward Swap")
oreex.decorate_plot(title="Example 10", ylabel="Exposure")
oreex.save_plot_to_file()

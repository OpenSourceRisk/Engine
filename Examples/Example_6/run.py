#!/usr/bin/env python

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

oreex = OreExample()

oreex.print_headline("Run ORE to produce NPV cube and exposures, without collateral")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")


oreex.setup_plot("plot_callable_swap")
oreex.plot("exposure_trade_204128.csv", 2, 3, 'b', "EPE Swap")
oreex.plot("exposure_trade_204128-swaption.csv", 2, 4, 'r', "ENE Swaption")
oreex.plot("exposure_nettingset_CUST_A.csv", 2, 3, 'g', "EPE Netting Set")
oreex.plot("exposure_trade_204128-short.csv", 2, 3, 'm', "EPE Short Swap")
oreex.decorate_plot(title="Example 6")
oreex.save_plot_to_file()

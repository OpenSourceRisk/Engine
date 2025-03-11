#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV cube and exposures, without collateral")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot results")

oreex.setup_plot("plot_callable_swap")
oreex.plot("exposure_trade_Swap.csv", 2, 3, 'b', "EPE Swap")
oreex.plot("exposure_trade_Swaption.csv", 2, 4, 'r', "ENE Swaption")
oreex.plot("exposure_nettingset_CPTY_A.csv", 2, 3, 'g', "EPE Netting Set")
oreex.plot("exposure_trade_ShortSwap.csv", 2, 3, 'm', "EPE Short Swap")
oreex.decorate_plot(title="Example 5")
oreex.save_plot_to_file()

#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV cube and exposures for CrossCurrencySwaps")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot results: Cross Currency Swap exposures, with and without FX reset")

oreex.setup_plot("example_xccy_reset")
oreex.plot("exposure_trade_XCCY_Swap_EUR_USD.csv", 2, 3, 'b', "Swap")
oreex.plot("exposure_trade_XCCY_Swap_EUR_USD_RESET.csv", 2, 3, 'r', "Resettable Swap")
oreex.decorate_plot(title="Example 9", legend_loc="upper left")
oreex.save_plot_to_file()

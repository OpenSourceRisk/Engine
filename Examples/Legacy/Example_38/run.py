#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV cube and exposures for Cross Currency Swaps")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot Cross Currency Swap results")

oreex.setup_plot("example_ccswap")
oreex.plot("exposure_trade_CCSwap.csv", 2, 3, 'b', "EPE CCSwap")
oreex.plot("exposure_trade_CCSwap.csv", 2, 4, 'r', "ENE CCSwap")
oreex.decorate_plot(title="Example 38")
oreex.save_plot_to_file()

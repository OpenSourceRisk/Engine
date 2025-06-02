#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV cube and exposures")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot results: Simulated exposures")

oreex.setup_plot("cmsspread")
oreex.plot("exposure_trade_CMS_Spread_Swap.csv", 2, 3, 'b', "Swap EPE")
oreex.plot("exposure_trade_CMS_Spread_Swap.csv", 2, 4, 'r', "Swap ENE")
oreex.decorate_plot(title="Example 28 - Simulated exposures for CMS Spread trades")
oreex.save_plot_to_file()


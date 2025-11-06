#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV cube and exposures, without collateral")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.setup_plot("example_swap_cash_physical")
oreex.plot("exposure_trade_Swap.csv", 2, 3, 'b', "EPE Swap")
oreex.plot("exposure_trade_SwaptionCash.csv", 2, 3, 'r', "EPE Swaption Cash")
oreex.plot("exposure_trade_SwaptionPhysical.csv", 2, 3, 'g', "EPE Swaption Physical")
oreex.plot("exposure_trade_SwaptionCashPremium.csv", 2, 3, 'c', "EPE Swaption Cash with Premium")
#oreex.plot("exposure_trade_SwaptionPhysicalPremium.csv", 2, 3, 'y', "EPE Swaption Physical with Premium")
oreex.decorate_plot(title="Example 3")
oreex.save_plot_to_file()

#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV cube and exposures, without collateral")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot results")

oreex.setup_plot("example_swaption")
oreex.plot("exposure_trade_SwaptionCash.csv", 2, 3, 'b', "EPE Swaption (Cash)")
oreex.plot("exposure_trade_SwaptionPhysical.csv", 2, 3, 'r', "EPE Swaption (Physical)")
oreex.plot("exposure_trade_Swap.csv", 2, 3, 'g', "EPE Forward Swap")
oreex.decorate_plot(title="Example 10", ylabel="Exposure")
oreex.save_plot_to_file()

#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV cube and exposures, without collateral")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot results")

oreex.setup_plot("example_swaption")
oreex.plot("exposure_trade_Swap.csv", 2, 3, 'b', "EPE Forward Swap")
oreex.plot("exposure_trade_SwaptionCash.csv", 2, 3, 'r', "EPE Bermudan Swaption (Cash)")
oreex.plot("exposure_trade_AmericanSwaptionCash.csv", 2, 3, 'm', "EPE American Swaption (Cash)")
oreex.plot("exposure_trade_SwaptionPhysical.csv", 2, 3, 'g', "EPE Bermudan Swaption (Physical)")
oreex.plot("exposure_trade_AmericanSwaptionPhysical.csv", 2, 3, 'y', "EPE American Swaption (Physical)")
oreex.decorate_plot(title="Example 4", ylabel="Exposure")
oreex.save_plot_to_file()

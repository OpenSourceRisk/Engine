#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE commodity example")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Run ORE commodity forward exposure")
oreex.run("Input/ore_wti.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot Forward exposure graph")
oreex.setup_plot("commodity_forward")
oreex.plot("exposure_trade_CommodityForward.csv", 2, 3, 'b', "EPE")
oreex.plot("exposure_trade_CommodityForward.csv", 2, 4, 'r', "ENE")
oreex.decorate_plot(title="Example 24 - Simulated Commodity Forward Exposure")
oreex.save_plot_to_file()

oreex.print_headline("Plot Option exposure graph")
oreex.setup_plot("commodity_option")
oreex.plot("exposure_trade_CommodityOption.csv", 2, 3, 'b', "EPE")
oreex.decorate_plot(title="Example 24 - Simulated Commodity Option Exposure")
oreex.save_plot_to_file()

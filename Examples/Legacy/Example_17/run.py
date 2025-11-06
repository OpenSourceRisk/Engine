#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot results: Simulated exposures")

oreex.setup_plot("CPI Swap")
oreex.plot("exposure_trade_CPI_Swap_1.csv", 2, 3, 'b', "EPE CPI Swap")
oreex.decorate_plot(title="Example 17", ylabel="Exposure")
oreex.save_plot_to_file()

oreex.setup_plot("YoY Swap")
oreex.plot("exposure_trade_YearOnYear_Swap.csv", 2, 3, 'b', "EPE YoY Swap")
oreex.decorate_plot(title="Example 17", ylabel="Exposure")
oreex.save_plot_to_file()

oreex.run("Input/ore_capfloor.xml")
oreex.get_times("Output/log_capfloor.txt")

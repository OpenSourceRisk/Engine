#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Run ORE to produce NPV cube and exposures")
oreex.run("Input/ore_exposure.xml")

oreex.print_headline("Plot results: Simulated exposures")

oreex.setup_plot("CPI Swap")
oreex.plot("exposure_trade_CPI_Swap.csv", 2, 3, 'b', "EPE CPI Swap")
oreex.decorate_plot(title="Example 17", ylabel="Exposure")
oreex.save_plot_to_file()

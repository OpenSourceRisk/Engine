#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV cube and exposures without horizon shift")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Run ORE to produce NPV cube and exposures with horizon shift")
oreex.run("Input/ore2.xml")

oreex.print_headline("Run ORE again to price European Swaptions")
oreex.run("Input/ore_swaption.xml")

oreex.print_headline("Plot results: Simulated exposures vs analytical swaption prices")

oreex.setup_plot("swaptions")
oreex.plot("exposure_trade_Swap_50y.csv", 2, 3, 'b', "Swap EPE (no horizon shift)")
oreex.plot("exposure_trade_Swap_50y.csv", 2, 4, 'r', "Swap ENE (no horizon shift)")
oreex.plot("exposure_trade_Swap_50y_2.csv", 2, 3, 'g', "Swap EPE (shifted horizon)")
oreex.plot("exposure_trade_Swap_50y_2.csv", 2, 4, 'y', "Swap ENE (shifted horizon)")
#oreex.plot_npv("swaption_npv.csv", 6, 'g', "NPV Swaptions", marker='s')

oreex.decorate_plot(title="Example 12 - Simulated exposures with and without horizon shift")
oreex.save_plot_to_file()

oreex.print_headline("Plot results: Zero rate distribution with and without shift")

oreex.setup_plot("rates")
oreex.plot_zeroratedist("scenariodump.csv", 0, 23, 5, 'r', label="No horizon shift", title="")
oreex.plot_zeroratedist("scenariodump2.csv", 0, 23, 5, 'b', label="Shifted horizon", title="Example 12 - 5y zero rate (EUR) distribution with and without horizon shift")
oreex.save_plot_to_file()


#!/usr/bin/env python

import os
import matplotlib.pyplot as plt
import runpy

ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

oreex = OreExample()

oreex.print_headline("Run ORE to produce NPV cube and exposures")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Run ORE again to price European Swaptions")
oreex.run("Input/ore_swaption.xml")


times_swaption = oreex.get_output_data_from_column("swaption_npv.csv", 3)
npv = oreex.get_output_data_from_column("swaption_npv.csv", 4)

fig = plt.figure()
ax = fig.add_subplot(111)

oreex.plot(ax, "exposure_trade_Swap_20y.csv", 2, 3, 'b', "EPE")
oreex.plot(ax, "exposure_trade_Swap_20y.csv", 2, 4, 'r', "ENE")
oreex.plot(ax, "swaption_npv.csv", 3, 4, 'r', "NPV")
oreex.decorate_plot(ax, title="Example 1 - Simulated exposures vs analytical swaption prices")

plt.show()

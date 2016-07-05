#!/usr/bin/env python

import os
import matplotlib.pyplot as plt
import runpy

ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

oreex = OreExample()

oreex.print_headline("Run ORE to produce NPV cube and exposures, without collateral")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

# plot: example_swaption
fig = plt.figure()
ax = fig.add_subplot(111)
oreex.plot(ax, "exposure_trade_SwaptionCash.csv", 2, 3, 'b', "EPE Swaption (Cash)")
oreex.plot(ax, "exposure_trade_SwaptionPhysical.csv", 2, 3, 'r', "EPE Swaption (Physical)")
oreex.plot(ax, "exposure_trade_Swap.csv", 2, 3, 'g', "EPE Forward Swap")
oreex.decorate_plot(ax, title="Example 10", ylabel="Exposure")

plt.show()

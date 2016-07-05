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

# plot: plot_callable_swap.pdf
fig = plt.figure()
ax = fig.add_subplot(111)
oreex.plot(ax, "exposure_trade_204128.csv", 2, 3, 'b', "EPE Swap")
oreex.plot(ax, "exposure_trade_204128-swaption.csv", 2, 4, 'r', "ENE Swaption")
oreex.plot(ax, "exposure_nettingset_CUST_A.csv", 2, 3, 'g', "EPE Netting Set")
oreex.plot(ax, "exposure_trade_204128-short.csv", 2, 3, 'm', "EPE Short Swap")
oreex.decorate_plot(ax, title="Example 6")

plt.show()

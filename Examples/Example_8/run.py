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

# plot: example_ccswap.pdf
fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_title("Example 8")
ax.set_xlabel("Time / Years")
ax.set_ylabel("Exposure")
swap_time_epe = oreex.get_output_data_from_column("exposure_trade_CCSwap.csv", 2)
swap_epe = oreex.get_output_data_from_column("exposure_trade_CCSwap.csv", 3)
swap_time_ene = oreex.get_output_data_from_column("exposure_trade_CCSwap.csv", 2)
swap_ene = oreex.get_output_data_from_column("exposure_trade_CCSwap.csv", 4)
ax.plot(swap_time_epe, swap_epe, color='b', label="EPE CCSwap")
ax.plot(swap_time_ene, swap_ene, color='r', label="ENE CCSwap")
ax.legend(loc='upper right', shadow=True)

plt.show()

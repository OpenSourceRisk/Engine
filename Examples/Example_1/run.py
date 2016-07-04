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

oreex.print_headline("Plot results: Simulated exposures vs analytical swaption prices")
time_swap = oreex.get_output_data_from_column("exposure_trade_Swap_20y.csv", 2)
epes = oreex.get_output_data_from_column("exposure_trade_Swap_20y.csv", 3)
enes = oreex.get_output_data_from_column("exposure_trade_Swap_20y.csv", 4)
times_swaption = oreex.get_output_data_from_column("swaption_npv.csv", 3)
npv = oreex.get_output_data_from_column("swaption_npv.csv", 4)

fig = plt.figure()
ax = fig.add_subplot(111)
ax.plot(time_swap, epes, color='b', label="EPE")
ax.plot(time_swap, enes, color='r', label="ENE")
ax.plot(times_swaption, npv, color='g', label="NPV")
ax.legend(loc='upper right', shadow=True)
ax.set_xlabel("Time / Years")
ax.set_ylabel("Exposure")
ax.set_title("Example 1")
plt.show()

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
ax.set_title("Example 6")
ax.set_xlabel("Time / Years")
ax.set_ylabel("Exposure")
swap_time = oreex.get_output_data_from_column("exposure_trade_204128.csv", 2)
swap_epe = oreex.get_output_data_from_column("exposure_trade_204128.csv", 3)
swaption_time = oreex.get_output_data_from_column("exposure_trade_204128-swaption.csv", 2)
swaption_ene = oreex.get_output_data_from_column("exposure_trade_204128-swaption.csv", 4)
ns_time = oreex.get_output_data_from_column("exposure_nettingset_CUST_A.csv", 2)
ns_epe = oreex.get_output_data_from_column("exposure_nettingset_CUST_A.csv", 3)
ss_time = oreex.get_output_data_from_column("exposure_trade_204128-short.csv", 2)
ss_epe = oreex.get_output_data_from_column("exposure_trade_204128-short.csv", 3)
ax.plot(swap_time, swap_epe, color='b', label="EPE Swap")
ax.plot(swaption_time, swaption_ene, color='r', label="ENE Swaption")
ax.plot(ns_time, ns_epe, color='g', label="EPE Netting Set")
ax.plot(ss_time , ss_epe, color='m', label="EPE Short Swap")
ax.legend(loc='upper right', shadow=True)

plt.show()

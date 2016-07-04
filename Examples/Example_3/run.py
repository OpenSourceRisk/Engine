#!/usr/bin/env python

import os
import matplotlib.pyplot as plt
import runpy

ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

oreex = OreExample()

oreex.print_headline("Run ORE to produce NPV cube and exposures")
#oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Run ORE to price European Payer Swaptions")
#oreex.run("Input/ore_payer_swaption.xml")

oreex.print_headline("Run ORE to price European Receiver Swaptions")
#oreex.run("Input/ore_receiver_swaption.xml")

fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_title("Example 3")
ax.set_xlabel("Time / Years")
ax.set_ylabel("Exposure")
swap_time = oreex.get_output_data_from_column("exposure_trade_Swap_20.csv", 2)
swap_epe = oreex.get_output_data_from_column("exposure_trade_Swap_20.csv", 3)
swap_ene = oreex.get_output_data_from_column("exposure_trade_Swap_20.csv", 4)
swaption_payer_npv = oreex.get_output_data_from_column("npv_payer.csv", 6)
swaption_receiver_npv = oreex.get_output_data_from_column("npv_receiver.csv", 6)
swaptions_times = range(1,len(swaption_payer_npv)+1)
ax.plot(swap_time, swap_epe, color='b', label="EPE")
ax.plot(swap_time, swap_ene, color='r', label="ENE")
ax.plot(swaptions_times, swaption_payer_npv, color='g', label="Payer Swaption")
ax.plot(swaptions_times, swaption_receiver_npv, color='m', label="Receiver Swaption")
ax.legend(loc='upper right', shadow=True)

plt.show()


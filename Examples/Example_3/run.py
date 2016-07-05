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

oreex.print_headline("Run ORE to price European Payer Swaptions")
oreex.run("Input/ore_payer_swaption.xml")

oreex.print_headline("Run ORE to price European Receiver Swaptions")
oreex.run("Input/ore_receiver_swaption.xml")

fig = plt.figure()
ax = fig.add_subplot(111)
oreex.plot(ax, "exposure_trade_Swap_20.csv", 2, 3, 'b', "EPE")
oreex.plot(ax, "exposure_trade_Swap_20.csv", 2, 4, 'r', "ENE")
oreex.plot_npv(ax, "npv_payer.csv", 6, 'g', 'Payer Swaption')
oreex.plot_npv(ax, "npv_receiver.csv", 6, 'm', 'Receiver Swaption')
oreex.decorate_plot(ax, title="Example 3")

plt.show()


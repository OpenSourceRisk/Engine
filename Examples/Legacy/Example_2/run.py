#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV cube and exposures")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Run ORE to price European Payer Swaptions")
oreex.run("Input/ore_payer_swaption.xml")

oreex.print_headline("Run ORE to price European Receiver Swaptions")
oreex.run("Input/ore_receiver_swaption.xml")

oreex.print_headline("Plot results: Simulated exposures vs analytical Swaption prices")

oreex.setup_plot("swaptions")
oreex.plot("exposure_trade_Swap_20.csv", 2, 3, 'b', "EPE")
oreex.plot("exposure_trade_Swap_20.csv", 2, 4, 'r', "ENE")
oreex.plot_npv("npv_payer.csv", 6, 'g', 'Payer Swaption', marker='s')
oreex.plot_npv("npv_receiver.csv", 6, 'm', "Receiver Swaption", marker='s')
oreex.decorate_plot(title="Example 2")
oreex.save_plot_to_file()

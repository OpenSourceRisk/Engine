#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV for equity derivatives")
oreex.run("Input/ore.xml")

oreex.print_headline("Plot results: Simulated exposures (call option)")

oreex.setup_plot("eq_call")
oreex.plot("exposure_trade_EqCall_SP5.csv", 2, 3, 'r', "Call EE")
oreex.plot("exposure_trade_EqCall_SP5.csv", 2, 4, 'g', "Call ENE")
oreex.plot("exposure_trade_EqCall_SP5.csv", 2, 7, 'b', "Call PFE(0.95)")
oreex.decorate_plot(title="Example 16 - Simulated exposures for SP5 call option")
oreex.save_plot_to_file()

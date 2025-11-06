#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV for equity derivatives")
oreex.run("Input/ore.xml")

oreex.print_headline("Plot results: Simulated exposures (Equity call option, forward, swap)")

oreex.setup_plot("eq_call")
oreex.plot("exposure_trade_EqCall_Luft.csv", 2, 3, 'r', "Call EPE")
oreex.plot("exposure_trade_EqForwardTrade_Luft.csv", 2, 3, 'b', "Fwd EPE")
oreex.decorate_plot(title="Example 16 - Simulated exposures for Luft call option and fwd trade")
oreex.save_plot_to_file()

#oreex.setup_plot("eq_swap")
#oreex.plot("exposure_trade_EquitySwap_1.csv", 2, 3, 'r', "Equity Swap 1 EPE")
#oreex.plot("exposure_trade_EquitySwap_2.csv", 2, 4, 'b', "Equity Swap 2 ENE")
#oreex.decorate_plot(title="Example 16 - Simulated exposures for Equity Swaps")
#oreex.save_plot_to_file()

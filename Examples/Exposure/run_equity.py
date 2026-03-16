#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposure: Equity                                 |")
print("+--------------------------------------------------+")

# Legacy example 16

oreex.run("Input/ore_equity.xml")
oreex.setup_plot("exposure_equity")
oreex.plot("equity/exposure_trade_EqCall_Luft.csv", 2, 3, 'r', "Call EPE")
oreex.plot("equity/exposure_trade_EqForwardTrade_Luft.csv", 2, 3, 'b', "Fwd EPE")
oreex.decorate_plot(title="Equity Forward and Option Exposure")
oreex.save_plot_to_file()

#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| AMC: Scripted Bermudan Swaption and and LPI Swap    |")
print("+-----------------------------------------------------+")

# Legacy example 54

oreex.print_headline("Run ORE to produce AMC exposure")
oreex.run("Input/ore_scriptedberm.xml")

oreex.print_headline("Plot results: Simulated exposure")

oreex.setup_plot("bermudanswaption")
oreex.plot("scriptedberm/exposure_trade_BermSwp.csv", 2, 3, 'b', "Swaption EPE")
oreex.plot("scriptedberm/exposure_trade_BermSwp.csv", 2, 4, 'r', "Swaption ENE")
oreex.decorate_plot(title="Scripting / AMC - Simulated exposure for Bermudan Swaption")
oreex.save_plot_to_file()

oreex.setup_plot("lpiswap")
oreex.plot("scriptedberm/exposure_trade_LPISwp.csv", 2, 3, 'b', "LPI Swap EPE")
oreex.plot("scriptedberm/exposure_trade_LPISwp.csv", 2, 4, 'r', "LPI Swap ENE")
oreex.decorate_plot(title="Scripting / AMC - Simulated exposure for LPI Swap")
oreex.save_plot_to_file()

oreex.setup_plot("dim_bermswp")
oreex.plot("scriptedberm/dim_evolution.csv", 8, 4, 'b', "DIM Evolution", filter='CPTY_A', filterCol=7)
oreex.decorate_plot(title="Scripting / AMC - DIM Evolution for Bermudan Swaption (sticky date mpor mode)")
oreex.save_plot_to_file()

oreex.setup_plot("dim_lpiswp")
oreex.plot("scriptedberm/dim_evolution.csv", 8, 4, 'b', "DIM Evolution", filter='CPTY_B', filterCol=7)
oreex.decorate_plot(title="Scripting / AMC - DIM Evolution for LPI Swap (sticky date mpor mode)")
oreex.save_plot_to_file()


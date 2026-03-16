#!/usr/bin/env python

import sys
import os
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce AMC exposure")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot results: Simulated exposure")

oreex.setup_plot("bermudanswaption")
oreex.plot("exposure_trade_BermSwp.csv", 2, 3, 'b', "Swaption EPE")
oreex.plot("exposure_trade_BermSwp.csv", 2, 4, 'r', "Swaption ENE")
oreex.decorate_plot(title="Example Scripting / AMC - Simulated exposure for Bermudan Swaption")
oreex.save_plot_to_file()

oreex.setup_plot("lpiswap")
oreex.plot("exposure_trade_LPISwp.csv", 2, 3, 'b', "LPI Swap EPE")
oreex.plot("exposure_trade_LPISwp.csv", 2, 4, 'r', "LPI Swap ENE")
oreex.decorate_plot(title="Example Scripting / AMC - Simulated exposure for LPI Swap")
oreex.save_plot_to_file()

oreex.setup_plot("dim_bermswp")
oreex.plot("dim_evolution.csv", 8, 4, 'b', "DIM Evolution", filter='CPTY_A', filterCol=7)
oreex.decorate_plot(title="Example Scripting / AMC - DIM Evolution for Bermudan Swaption (sticky date mpor mode)")
oreex.save_plot_to_file()

oreex.setup_plot("dim_lpiswp")
oreex.plot("dim_evolution.csv", 8, 4, 'b', "DIM Evolution", filter='CPTY_B', filterCol=7)
oreex.decorate_plot(title="Example Scripting / AMC - DIM Evolution for LPI Swap (sticky date mpor mode)")
oreex.save_plot_to_file()

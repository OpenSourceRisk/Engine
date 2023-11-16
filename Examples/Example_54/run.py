#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce AMC CG exposure")
oreex.run("Input/ore.xml")

oreex.print_headline("Plot results: Simulated exposure")

oreex.setup_plot("amc_vanillaswap_eur")
oreex.plot("exposure_trade_Swap_EUR.csv", 2, 3, 'b', "AMC Vanilla Swap EUR EPE")
oreex.decorate_plot(title="Example 39 - Simulated exposure for 20y EUR Payer Swap")
oreex.save_plot_to_file()

#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| AMC: Forward Bond                                   |")
print("+-----------------------------------------------------+")

# Legacy example 73

oreex.print_headline("Run ORE to produce AMC Forward Bond exposure")
oreex.run("Input/ore_forwardbond.xml")

oreex.print_headline("Plot results: Simulated exposure")

oreex.setup_plot("forwardbond")
oreex.plot("forwardbond/exposure_trade_FwdBond.csv", 2, 3, 'b', "Forward Bonds EPE")
oreex.plot("forwardbond/exposure_trade_FwdBond.csv", 2, 4, 'r', "Forward Bond ENE")
oreex.decorate_plot(title="Scripting / AMC - Simulated Forward Bond Exposure")
oreex.save_plot_to_file()

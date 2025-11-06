#!/usr/bin/env python

import os
import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce AMC exposure")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot results: Simulated exposure")

oreex.setup_plot("TaRF")
oreex.plot("exposure_trade_SCRIPTED_FX_TARF.csv", 2, 3, 'b', "TaRF EPE")
oreex.plot("exposure_trade_SCRIPTED_FX_TARF.csv", 2, 4, 'r', "TaRF ENE")
oreex.decorate_plot(title="Example Scripting / AMC - Simulated exposure for TaRF")
oreex.save_plot_to_file()

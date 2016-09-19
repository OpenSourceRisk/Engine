#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV cube and exposures")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot results: ORE vs QRE exposures")

oreex.setup_plot("ore_qre_comparison")
oreex.plot("exposure_trade_627291AI.csv", 2, 3, 'b', "ORE EPE")
oreex.plot("exposure_trade_627291AI.csv", 2, 4, 'r', "ORE ENE")
oreex.plot("20160301_qre_exposure_trade627291AI.csv", 6, 7, 'g', "QRE EPE")
oreex.plot("20160301_qre_exposure_trade627291AI.csv", 6, 8, 'g', "QRE ENE")
oreex.decorate_plot(title="Example 0: ORE vs QRE Swap Exposure")
oreex.save_plot_to_file()


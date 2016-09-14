#!/usr/bin/env python

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

import sys
oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV cube and exposures")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")
 
oreex.print_headline("Plot results")
 
oreex.setup_plot("Basel Measures")
oreex.plot("exposure_trade_Swap.csv", 2, 3, 'b', "EPE")
oreex.plot("exposure_trade_Swap.csv", 2, 8 ,'c', "BASEL EE")
oreex.plot("exposure_trade_Swap.csv", 2, 9 ,'r', "BASEL EEE")

epe = oreex.get_output_data_from_column("xva.csv", 12, 1)
oreex.plot_hline(int(epe[0]), 'g', "BASEL EPE")

eepe = oreex.get_output_data_from_column("xva.csv", 13, 1)
oreex.plot_hline(int(eepe[0]), 'y', "BASEL EEPE")

oreex.decorate_plot(title="Example 11 - Basel Measures")
oreex.save_plot_to_file()

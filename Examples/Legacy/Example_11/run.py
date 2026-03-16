#!/usr/bin/env python

import os
import runpy
import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce NPV cube and exposures")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")
 
oreex.print_headline("Plot results")
 
oreex.setup_plot("BaselMeasures")
oreex.plot("exposure_trade_Swap.csv", 2, 3, 'b', "EPE")
oreex.plot("exposure_trade_Swap.csv", 2, 8 ,'c', "BASEL EE")
oreex.plot("exposure_trade_Swap.csv", 2, 9 ,'r', "BASEL EEE")

epe = oreex.get_output_data_from_column("xva.csv", 20, 1)
from decimal import Decimal
oreex.plot_hline(Decimal(epe[0]), 'g', "BaselEPE")

eepe = oreex.get_output_data_from_column("xva.csv", 21, 1)
oreex.plot_hline(Decimal(eepe[0]), 'y', "BaselEEPE")

oreex.decorate_plot(title="Example 11 - Basel Measures")
oreex.save_plot_to_file()

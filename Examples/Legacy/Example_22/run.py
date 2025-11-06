#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE for Sensitivity Analysis (simulating full volatility surfaces)")
oreex.run("Input/ore_fullSurface.xml")
oreex.get_times("Output/log_fullSurface.txt")

oreex.print_headline("Run ORE for Sensitivity Analysis (simulating volatility atm strikes only)")
oreex.run("Input/ore_atmOnly.xml")
oreex.get_times("Output/log_atmOnly.txt")




#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE for Sensitivity, Stress and Parametric VaR Analysis")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Run ORE for Parametric VaR Analysis")
oreex.run("Input/ore_var.xml")
oreex.get_times("Output/log_var.txt")



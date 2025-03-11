#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE for Sensitivity Analysis (including Par conversion), cpr = 0%")
oreex.run("Input/ore_0.xml")
oreex.get_times("Output/log_0.txt")

oreex.print_headline("Run ORE for Sensitivity Analysis (including Par conversion), cpr = 5%")
oreex.run("Input/ore_5.xml")
oreex.get_times("Output/log_5.txt")

oreex.print_headline("Run ORE for Sensitivity Analysis (including Par conversion), cpr = 10%")
oreex.run("Input/ore_10.xml")
oreex.get_times("Output/log_10.txt")

oreex.print_headline("Run ORE for Sensitivity Analysis (including Par conversion), cpr = 15%")
oreex.run("Input/ore_15.xml")
oreex.get_times("Output/log_15.txt")

oreex.print_headline("Run ORE for Sensitivity Analysis (including Par conversion), cpr = 20%")
oreex.run("Input/ore_20.xml")
oreex.get_times("Output/log_20.txt")

oreex.print_headline("Run ORE for Sensitivity Analysis (including Par conversion), cpr = 50%")
oreex.run("Input/ore_50.xml")
oreex.get_times("Output/log_50.txt")

oreex.print_headline("Run ORE for Sensitivity Analysis (including Par conversion), cpr = 0% - 20%")
oreex.run("Input/ore_0_20.xml")
oreex.get_times("Output/log_0_20.txt")

oreex.print_headline("Run ORE for Sensitivity Analysis (including Par conversion), cpr = 0% - 50%")
oreex.run("Input/ore_0_50.xml")
oreex.get_times("Output/log_0_50.txt")

oreex.print_headline("Run ORE for Sensitivity Analysis (including Par conversion), cpr = 0% - 100%")
oreex.run("Input/ore_0_100.xml")
oreex.get_times("Output/log_0_100.txt")


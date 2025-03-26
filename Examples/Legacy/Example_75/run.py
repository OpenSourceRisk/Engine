#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce scenarios and amc buffers")
oreex.run("Input/ore_scenario.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Run ORE to produce NPV cube and exposures with horizon shift for classic case")
oreex.run("Input/ore_classic.xml")

oreex.print_headline("Run ORE to produce NPV cube and exposures with horizon shift for amc case")
oreex.run("Input/ore_amc.xml")



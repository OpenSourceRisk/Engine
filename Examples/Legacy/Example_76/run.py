#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE for maturing portfolio with includeTodaysCashFlows=true")
oreex.run("Input/ore_maturing.xml")

oreex.print_headline("Run classic simulation for short term portfolio with includeTodaysCashFlows=true")
oreex.run("Input/ore_classic.xml")

oreex.print_headline("Run AMC simulation for short term portfolio with includeTodaysCashFlows=true")
oreex.run("Input/ore_amc.xml")


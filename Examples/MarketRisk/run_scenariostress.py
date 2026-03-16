#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| Scenario Stress Test                   |")
print("+----------------------------------------+")

oreex.print_headline("Run ORE for Scenario Stress testing")
oreex.run("Input/ore_scenario_stress.xml")


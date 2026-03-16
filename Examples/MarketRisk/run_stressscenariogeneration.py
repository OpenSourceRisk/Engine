#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| Scenario Generation Test                            |")
print("+----------------------------------------+")

oreex.print_headline("Run ORE for Stress Scenario Generation testing")
oreex.run("Input/ore_stress_scenario_generation.xml")


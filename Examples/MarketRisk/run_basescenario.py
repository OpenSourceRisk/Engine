#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| Base Scenario Export                   |")
print("+----------------------------------------+")

# legacy example 57

oreex.print_headline("Run ORE to export the base scenario")
oreex.run("Input/ore_basescenario.xml")

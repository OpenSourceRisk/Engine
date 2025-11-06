#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| Base Scenario Export                   |")
print("+----------------------------------------+")

# legacy example 69

oreex.print_headline("Run ORE convert zero shifts to par rate shifts")
oreex.run("Input/ore_zerotoparshift.xml")

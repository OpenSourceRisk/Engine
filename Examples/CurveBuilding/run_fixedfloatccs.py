#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Discount Ratio Curves                               |")
print("+-----------------------------------------------------+")

# Legacy example 29

oreex.print_headline("Run ORE to illustrate use of fixed vs float cross currency swap helpers")
oreex.run("Input/ore_fixedfloatccs.xml")

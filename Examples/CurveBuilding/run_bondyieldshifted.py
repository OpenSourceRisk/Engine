#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Curve Building with Shifted Bond Yields             |")
print("+-----------------------------------------------------+")

# Legacy example 49

oreex.print_headline("Run ORE to produce Bond NPV with BondYieldShifted curve setup")
oreex.run("Input/ore_bondyieldshifted.xml")

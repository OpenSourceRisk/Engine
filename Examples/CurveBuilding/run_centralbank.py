#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Curve Building with Central Bank Meeting Dates      |")
print("+-----------------------------------------------------+")

# Legacy example 53

oreex.run("Input/ore_centralbank.xml")

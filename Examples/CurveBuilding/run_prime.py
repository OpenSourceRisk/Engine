#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Discount Ratio Curves                               |")
print("+-----------------------------------------------------+")

# Legacy example 30

oreex.print_headline("Run ORE to build USD-Prime curve.")
oreex.run("Input/ore_prime.xml")

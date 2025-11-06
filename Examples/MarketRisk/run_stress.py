#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| Stress Test                            |")
print("+----------------------------------------+")

# legacy example 15 and 77

oreex.print_headline("Run ORE for Stress testing")
oreex.run("Input/ore_stress.xml")


#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| Stand-alone par conversion analytic    |")
print("+----------------------------------------+")

# legacy example 50

oreex.print_headline("Run ORE for Par Sensitivity Conversion Analysis")
oreex.run("Input/ore_parconversion.xml")


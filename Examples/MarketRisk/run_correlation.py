#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| Correlation Analytic                   |")
print("+----------------------------------------+")

oreex.print_headline("Run ORE for Correlation Analytic")
oreex.run("Input/ore_correlation.xml")

#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| Parametric VaR                         |")
print("+----------------------------------------+")

# legacy example 15

oreex.print_headline("Run ORE for Parametric VaR")
oreex.run("Input/ore_parametricvar.xml")


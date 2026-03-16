#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| Sensitivity Analysis                   |")
print("+----------------------------------------+")

# legacy example 22

oreex.print_headline("Run ORE for Sensitivity Analysis (simulating full volatility surfaces)")
oreex.run("Input/ore_sensismile_surface.xml")

oreex.print_headline("Run ORE for Sensitivity Analysis (simulating volatility atm strikes only)")
oreex.run("Input/ore_sensismile_atm.xml")

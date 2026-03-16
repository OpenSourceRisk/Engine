#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Discount Ratio Curves                               |")
print("+-----------------------------------------------------+")

# Legacy example 28

oreex.print_headline("Run with USD base currency")
oreex.run("Input/ore_discountratio_usd.xml")

oreex.print_headline("Run with EUR base currency using GBP-IN-EUR discount modified ratio curve")
oreex.run("Input/ore_discountratio_eur.xml")

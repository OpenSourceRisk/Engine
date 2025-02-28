#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| Sensitivity Analysis                   |")
print("+----------------------------------------+")

# legacy examples 15 and 40
oreex.print_headline("Run ORE for Sensitivity")
oreex.run("Input/ore_sensi.xml")

#oreex.print_headline("Run ORE for Parametric VaR Analysis")
#oreex.run("Input/ore_var.xml")



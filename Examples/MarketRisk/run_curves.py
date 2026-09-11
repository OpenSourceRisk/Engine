#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------+")
print("| Curves Analytic |")
print("+-----------------+")


oreex.print_headline("Run ORE for Curves Analytic")
oreex.run("Input/ore_curves.xml")



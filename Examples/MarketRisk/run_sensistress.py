#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| Stressed Sensitivity Analysis                   |")
print("+----------------------------------------+")

oreex.print_headline("Run ORE for Stressed Sensitivity")
oreex.run("Input/ore_sensistress.xml")



#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| Hist Sim VaR                           |")
print("+----------------------------------------+")

# legacy example 58

oreex.print_headline("Run ORE for HistSim VaR")
oreex.run("Input/ore_histsimvar.xml")


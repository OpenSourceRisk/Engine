#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| Market Risk for IntradayPower products |")
print("+----------------------------------------+")

oreex.print_headline("Run ORE for NPV, Sensitivity and Stresstest")
oreex.run("Input/ore_intradaypower.xml")

oreex.print_headline("Run ORE for NPV, Sensitivity and Stresstest with curve algebra")
oreex.run("Input/ore_intradaypower_curvealgebra.xml")

oreex.print_headline("Run ORE for hist sim var with curve algebra")
oreex.run("Input/ore_intradaypower_histsimvar.xml")



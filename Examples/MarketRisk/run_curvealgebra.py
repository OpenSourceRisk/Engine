#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-------------------------------------------------------------------------------+")
print("| Sensitivity ans Stress Analysis with curve algebra for commodity basis curves |")
print("+-------------------------------------------------------------------------------+")


oreex.print_headline("Run ORE for Sensitivity with curve algebra for commodity basis curves")
oreex.run("Input/ore_curvealgebra.xml")



#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Collateral: First MPoR Adjustment                   |")
print("+-----------------------------------------------------+")

# Legacy example 72

oreex.print_headline("Run ORE with firstMporCollateralAdjustment=true")
oreex.run("Input/ore_firstmpor.xml")

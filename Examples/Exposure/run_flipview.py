#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposure: Flip View                              |")
print("+--------------------------------------------------+")

# Legacy example 35

oreex.print_headline("Run ORE to produce normal xva from BANKs view")
oreex.run("Input/ore_flipview_normal.xml")

oreex.print_headline("Run ORE to produce flipped xva from CPTY_A's view")
oreex.run("Input/ore_flipview.xml")

oreex.print_headline("Run ORE to produce normal xva with reversed swap (identical to flipped xva as BANK and CPTY_A have identical Default and fva curves)")
oreex.run("Input/ore_flipview_reversed.xml")

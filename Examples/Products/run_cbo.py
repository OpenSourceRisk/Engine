#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Products: CBO                                       |")
print("+-----------------------------------------------------+")

oreex.print_headline("Run ORE to price the CBO example")
oreex.run("Input/CBO/ore.xml")

#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Dynamic SIMM Regression Testing                     |")
print("+-----------------------------------------------------+")

oreex.print_headline("Run ORE Dynamic SIMM Regression Testing")
oreex.run("Input/Dim2/ore_amccg.xml")

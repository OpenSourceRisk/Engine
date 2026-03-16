#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| AMC Legacy      |")
print("+----------------------------------------+")

oreex.print_headline("Run AMC Legacy")
oreex.run("Input/ore_amc_legacy.xml")


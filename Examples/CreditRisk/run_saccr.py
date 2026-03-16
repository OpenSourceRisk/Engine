#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| SA-CCR                                              |")
print("+-----------------------------------------------------+")

# Legacy example 68, SA-CCR part

oreex.print_headline("Run SA-CCR")
oreex.run("Input/ore_saccr.xml")

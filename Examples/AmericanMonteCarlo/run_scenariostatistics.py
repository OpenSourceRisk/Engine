#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| AMC: Scenario statistics and amc buffers            |")
print("+-----------------------------------------------------+")

# Legacy example 75

oreex.run("Input/ore_scenariostatistics.xml")

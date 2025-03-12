#!/usr/bin/env python

import glob
import os
import sys
import utilities
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Dynamic IM and MVA, with AMC and AAD                |")
print("+-----------------------------------------------------+")

oreex.print_headline("Run ORE to generate a scenario dump")
#oreex.run("Input/Dim2/ore.xml")

oreex.print_headline("Convert scenario dump into series of market data files")
utilities.scenarioToMarket('Dim2')

oreex.print_headline("Run ORE on a future implied market")
oreex.run("Input/DimValidation/ore.xml")





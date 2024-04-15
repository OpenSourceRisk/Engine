#!/usr/bin/env python

import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

samples1=os.environ["OVERWRITE_SCENARIOGENERATOR_SAMPLES"]
print("samples1 =", samples1)
os.environ["OVERWRITE_SCENARIOGENERATOR_SAMPLES"]=""
samples2=os.environ["OVERWRITE_SCENARIOGENERATOR_SAMPLES"]
print("samples2 =", samples2)

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce AMC exposure")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

os.environ["OVERWRITE_SCENARIOGENERATOR_SAMPLES"]=samples1

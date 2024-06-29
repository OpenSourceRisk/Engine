#!/usr/bin/env python

import sys
import os
sys.path.append('../')
from ore_examples_helper import OreExample

if "OVERWRITE_SCENARIOGENERATOR_SAMPLES" in os.environ.keys() :
    samples1=os.environ["OVERWRITE_SCENARIOGENERATOR_SAMPLES"]
    print("samples1 =", samples1)
    os.environ["OVERWRITE_SCENARIOGENERATOR_SAMPLES"]=""
    samples2=os.environ["OVERWRITE_SCENARIOGENERATOR_SAMPLES"]
    print("samples2 =", samples2)

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE with firstMporCollateralAdjustment")
oreex.run("Input/ore.xml")


#!/usr/bin/env python

import sys
import os
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

# use full sample size for amc, even if overriden
if "OVERWRITE_SCENARIOGENERATOR_SAMPLES" in os.environ.keys() :
    samples1=os.environ["OVERWRITE_SCENARIOGENERATOR_SAMPLES"]
    os.environ["OVERWRITE_SCENARIOGENERATOR_SAMPLES"]=""

oreex.print_headline("Run ORE to produce XVA Stresstest with AMC")
oreex.run("Input/ore_amc.xml")
#oreex.get_times("Output/log.txt")

# restore original sample size, if it was set before
if "OVERWRITE_SCENARIOGENERATOR_SAMPLES" in os.environ.keys() :
    os.environ["OVERWRITE_SCENARIOGENERATOR_SAMPLES"]=samples1

oreex.print_headline("Run ORE to produce XVA Stresstest with classic simulation")
oreex.run("Input/ore_classic.xml")

#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce AMC CG exposure")

oreex.run("Input/ore_ad.xml")
oreex.run("Input/ore_bump.xml")
oreex.run("Input/ore_gpu.xml")
oreex.run("Input/ore_cgcube.xml")
oreex.run("Input/ore_cgcube_gpu.xml")
oreex.run("Input/ore_amc_legacy.xml")


#!/usr/bin/env python3

import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE Bump")
oreex.run("Input/ore_bump.xml")
oreex.get_times("Output/bump/log.txt")

oreex.print_headline("Run ORE AD")
oreex.run("Input/ore_ad.xml")
oreex.get_times("Output/ad/log.txt")


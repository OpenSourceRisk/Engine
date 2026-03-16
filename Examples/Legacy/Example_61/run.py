#!/usr/bin/env python3

import os
import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE Bump Sensitivities")
oreex.run("Input/ore.xml")
oreex.get_times("Output/bump/log.txt")

oreex.print_headline("Run ORE with CG")
oreex.run("Input/ore_cg.xml")
oreex.get_times("Output/cg/log.txt")

oreex.print_headline("Run ORE AD")
oreex.run("Input/ore_ad.xml")
oreex.get_times("Output/ad/log.txt")

oreex.print_headline("Run ORE on GPU")
oreex.run("Input/ore_gpu.xml")
oreex.get_times("Output/gpu/log.txt")


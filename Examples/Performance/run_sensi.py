#!/usr/bin/env python3

import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| Exotics Sensis using AAD and GPU       |")
print("+----------------------------------------+")

oreex.print_headline("Run ORE Bump Sensitivities")
oreex.run("Input/ore_sensi.xml")

oreex.print_headline("Run ORE with CG")
oreex.run("Input/ore_sensi_cg.xml")

oreex.print_headline("Run ORE AD")
oreex.run("Input/ore_sensi_ad.xml")

oreex.print_headline("Run ORE on GPU")
oreex.run("Input/ore_sensi_gpu.xml")


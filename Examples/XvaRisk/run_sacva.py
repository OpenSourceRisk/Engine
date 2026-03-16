#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| XVA Risk: SA-CVA                                    |")
print("+-----------------------------------------------------+")

# Legacy example 68, SA-CVA part

oreex.print_headline("Run SA-CVA with AMC CVA Sensitivities")
oreex.run("Input/ore_sacva.xml")

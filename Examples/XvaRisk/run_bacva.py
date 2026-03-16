#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| XVA Risk: BA-CVA                                    |")
print("+-----------------------------------------------------+")

# Legacy example 68, BA-CVA part

oreex.print_headline("Run BA-CVA")
oreex.run("Input/ore_bacva.xml")

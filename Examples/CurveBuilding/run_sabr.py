#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| SABR Swaption and Cap/Floor Smile                   |")
print("+-----------------------------------------------------+")

oreex.print_headline("Run ORE Swaption SABR example")
oreex.run("Input/ore_sabr.xml")

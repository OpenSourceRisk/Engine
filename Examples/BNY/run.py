#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| BNY Example                                         |")
print("+-----------------------------------------------------+")

# Legacy example 68, SA-CCR part

oreex.print_headline("Run BNY Example")
oreex.run("Input/ore.xml")

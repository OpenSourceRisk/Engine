#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+------------------------------------------+")
print("| Basic Standard Market Risk Capital, SMRC |")
print("+------------------------------------------+")

# small part of legacy example 68

oreex.print_headline("Run ORE for SMRC")
oreex.run("Input/ore_smrc.xml")


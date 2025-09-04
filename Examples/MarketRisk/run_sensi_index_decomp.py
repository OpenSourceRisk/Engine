#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+------------------------------------------------------+")
print("| Sensitivity Analysis with credit index decomposition |")
print("+------------------------------------------------------+")


oreex.print_headline("Run ORE for Sensitivity with credit index decomposition")
oreex.run("Input/ore_sensi_eq_credit_index_decomposition.xml")



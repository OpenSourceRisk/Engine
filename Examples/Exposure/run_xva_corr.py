#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Exposure: Cross Ccy Swaps with Correlation Analytic |")
print("+-----------------------------------------------------+")

# Legacy Examples 8 and 9 combined

oreex.run("Input/ore_xva_corr.xml")





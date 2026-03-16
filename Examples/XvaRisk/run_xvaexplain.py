#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| XVA Explain                                         |")
print("+-----------------------------------------------------+")

# Legacy example 70
# TODO: Consolidate with the XVA Sensitivity example (portfolio, market data, configuration)

oreex.print_headline("Run XVA Explain")
oreex.run("Input/ore_explain.xml")

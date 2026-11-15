#!/usr/bin/env python

import sys
sys.path.append('../../../')
sys.path.append('..')
from ore_examples_helper import OreExample
from multiple_cuts import MultipleCuts

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+------------------------------------------------------+")
print("| Multiple Cuts, Commodity: Gold Vol Pillars at 2 Cuts |")
print("+------------------------------------------------------+")

MultipleCuts(oreex).run("multiple_cuts_commodity.csv")

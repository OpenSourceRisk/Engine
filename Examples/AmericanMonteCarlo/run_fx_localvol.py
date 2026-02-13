#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------------------------------------+")
print("| Exposures: FX Forward/Option with LocalVol (deterministic interest rate)       |")
print("+--------------------------------------------------------------------------------+")

oreex.run("Input/ore_fx_localvol.xml")
oreex.run("Input/ore_fx_localvol_npv.xml")

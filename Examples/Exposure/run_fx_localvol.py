#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------------------------------------------------------+")
print("| Exposures: FX Forward/Option with LocalVol (det, stoch rates and comparison with fx option npvs     |")
print("+-----------------------------------------------------------------------------------------------------+")

oreex.run("Input/ore_fx_localvol_detrates.xml")
oreex.run("Input/ore_fx_localvol_stochrates.xml")
oreex.run("Input/ore_fx_localvol_npv.xml")

#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE PnL")
oreex.run("Input/ore_pnl.xml")

oreex.print_headline("Run ORE PNL Explain")
oreex.run("Input/ore_explain.xml")

oreex.print_headline("Run ORE PNL Explain with Par Rates")
oreex.run("Input/ore_explain_par.xml")

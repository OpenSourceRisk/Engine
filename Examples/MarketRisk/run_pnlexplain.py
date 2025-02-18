#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| P&L and P&L Explain                    |")
print("+----------------------------------------+")

# legacy example 62

oreex.print_headline("Run ORE PnL")
oreex.run("Input/ore_pnl.xml")

oreex.print_headline("Run ORE PNL Explain")
oreex.run("Input/ore_pnlexplain.xml")

oreex.print_headline("Run ORE PNL Explain with Par Rates")
oreex.run("Input/ore_pnlexplain_par.xml")


#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposures: Callable Bond.                        |")
print("+--------------------------------------------------+")

# Legacy example 6

oreex.run("Input/ore_callable_bond.xml")

oreex.setup_plot("exposure_callable_bond")
oreex.plot("callable_bond/exposure_trade_CallableBondNoCall.csv", 2, 3, 'b', "EPE NeverCall")
oreex.plot("callable_bond/exposure_trade_CallableBondCertainCall.csv", 2, 3, 'g', "EPE CertainCall")
oreex.plot("callable_bond/exposure_trade_CallableBondTrade.csv", 2, 3, 'r', "EPE CallableBond")
oreex.decorate_plot(title="Callable Bond Exposure")
oreex.save_plot_to_file()




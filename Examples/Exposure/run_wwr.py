#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposure: Wrong Way Risk                         |")
print("+--------------------------------------------------+")

# Legacy example 34

oreex.run("Input/ore_wwr.xml")

oreex.setup_plot("exposure_wwr")
oreex.plot("wwr/exposure_trade_Swap_20.csv", 2, 3, 'b', "Swap EPE")
oreex.plot("wwr/exposure_trade_Swap_20.csv", 2, 4, 'r', "Swap ENE")
oreex.decorate_plot(title="Simulated Swap Exposure with non-zero IR:CR correlation")
oreex.save_plot_to_file()

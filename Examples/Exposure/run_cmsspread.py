#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposure: CMS Spread Swap                        |")
print("+--------------------------------------------------+")

# Legacy example 25

oreex.run("Input/ore_cmsspread.xml")

oreex.print_headline("Plot results")

oreex.setup_plot("exposure_cmsspread")
oreex.plot("cmsspread/exposure_trade_CMS_Spread_Swap.csv", 2, 3, 'b', "Swap EPE")
oreex.plot("cmsspread/exposure_trade_CMS_Spread_Swap.csv", 2, 4, 'r', "Swap ENE")
oreex.decorate_plot(title="Exposures for a CMS Spread Swap")
oreex.save_plot_to_file()


#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposure: FRA and Average OIS Swaps              |")
print("+--------------------------------------------------+")

# Legacy example 23

oreex.run("Input/ore_fra.xml")
oreex.setup_plot("exposure_fra")
oreex.plot("fra/exposure_trade_fra1.csv", 2, 3, 'r', "FRA EPE")
oreex.plot("fra/exposure_trade_averageOIS.csv", 2, 3, 'b', "Average OIS Swap EPE")
oreex.decorate_plot(title="FRA and Average OIS Swap Exposure")
oreex.save_plot_to_file()

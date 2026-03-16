#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposures: Cap/Floor/Collar                      |")
print("+--------------------------------------------------+")

# Legacy example 6

oreex.run("Input/ore_capfloor.xml")

oreex.setup_plot("exposure_capfloor")
oreex.plot("capfloor/exposure_trade_floor_01.csv", 2, 3, 'b', "EPE Floor")
oreex.plot("capfloor/exposure_trade_cap_01.csv", 2, 4, 'r', "ENE Cap")
oreex.plot("capfloor/exposure_nettingset_CPTY_B.csv", 2, 3, 'g', "EPE Net Cap and Floor")
oreex.plot("capfloor/exposure_trade_collar_02.csv", 2, 4, 'g', "ENE Collar", offset=1, marker='o', linestyle='')
oreex.decorate_plot(title="Cap/Floor/Collar Exposure")
oreex.save_plot_to_file()




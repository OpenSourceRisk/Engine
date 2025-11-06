#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposure: Commodity                              |")
print("+--------------------------------------------------+")

# Legacy Example 24

oreex.run("Input/ore_commodity.xml")
oreex.setup_plot("exposure_commodity")
oreex.plot("commodity/exposure_trade_CommodityForward.csv", 2, 3, 'r', "Forward EPE")
#oreex.plot("commodity/exposure_trade_CommodityForward.csv", 2, 4, 'y', "Forward ENE")
oreex.plot("commodity/exposure_trade_CommodityOption.csv", 2, 3, 'g', "Option EPE")
oreex.plot("commodity/exposure_trade_CommoditySwap.csv", 2, 3, 'b', "Swap EPE")
#oreex.plot("commodity/exposure_trade_CommoditySwap.csv", 2, 4, 'm', "Swap ENE")
#oreex.plot("commodity/exposure_trade_CommodityAPO.csv", 2, 3, 'b', "APO EPE")
#oreex.plot("commodity/exposure_trade_CommoditySwaption.csv", 2, 3, 'b', "Swaption EPE")
oreex.decorate_plot(title="Commodity Derivatives Exposure")
oreex.save_plot_to_file()

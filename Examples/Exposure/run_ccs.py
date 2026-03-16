#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposure: Cross Ccy Swaps                        |")
print("+--------------------------------------------------+")

# Legacy Examples 8 and 9 combined

oreex.run("Input/ore_ccs.xml")

oreex.setup_plot("exposure_ccs")
oreex.plot("ccs/exposure_trade_CCSwap.csv", 2, 3, 'b', "CC Swap EPE")
oreex.plot("ccs/exposure_trade_CCSwap.csv", 2, 4, 'r', "CC Swap ENE")
oreex.decorate_plot(title="Cross Currency Swap without Reset", legend_loc="upper left")
oreex.save_plot_to_file()

oreex.setup_plot("exposure_ccsreset")
oreex.plot("ccs/exposure_trade_XCCY_Swap_EUR_USD.csv", 2, 3, 'b', "CC Swap EPE")
oreex.plot("ccs/exposure_trade_XCCY_Swap_EUR_USD_RESET.csv", 2, 3, 'r', "Resettable CC Swap EPE")
oreex.decorate_plot(title="Cross Currency Swaps with Reset vs without Reset", legend_loc="upper left")
oreex.save_plot_to_file()





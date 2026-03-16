#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposure: Single Swap with Normal Market         |")
print("+--------------------------------------------------+")

# Legacy example 2

oreex.run("Input/ore_swap.xml")
oreex.run("Input/ore_swap_swaptions.xml")

oreex.setup_plot("exposure_swap")
oreex.plot("swap/exposure_trade_Swap_20.csv", 2, 3, 'b', "Swap EPE")
oreex.plot("swap/exposure_trade_Swap_20.csv", 2, 4, 'r', "Swap ENE")
oreex.plot_npv("swap/npv_swaptions.csv", 6, 'g', "Payer Swaption NPV", marker='s', filter='Payer_Swaption', filterCol=0)
oreex.plot_npv("swap/npv_swaptions.csv", 6, 'm', "Receiver Swaption NPV", marker='s', filter='Receiver_Swaption', filterCol=0)
oreex.decorate_plot(title="Swap Exposures vs Analytical Swaption Prices")
oreex.save_plot_to_file()

#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+------------------------------------------------+")
print("| Exposure: Single Swap with Flat Market, Basel  |")
print("+------------------------------------------------+")

# Legacy examples 1 and 11

oreex.run("Input/ore_swapflat.xml")
oreex.run("Input/ore_swapflat_swaptions.xml")

oreex.setup_plot("exposure_swapflat")
oreex.plot("swapflat/exposure_trade_Swap_20.csv", 2, 3, 'b', "Swap EPE")
oreex.plot("swapflat/exposure_trade_Swap_20.csv", 2, 4, 'r', "Swap ENE")
oreex.plot_npv("swapflat/npv_swaptions.csv", 6, 'g', "NPV Swaptions", marker='s', filter='Swaption', filterCol=1)
oreex.decorate_plot(title="Swap Exposures vs Analytical Swaption Prices")
oreex.save_plot_to_file()

oreex.setup_plot("BaselMeasures")
oreex.plot("swapflat/exposure_trade_Swap_20.csv", 2, 3, 'b', "EPE")
oreex.plot("swapflat/exposure_trade_Swap_20.csv", 2, 8 ,'c', "BASEL EE")
oreex.plot("swapflat/exposure_trade_Swap_20.csv", 2, 9 ,'r', "BASEL EEE")

epe = oreex.get_output_data_from_column("swapflat/xva.csv", 20, 1)
from decimal import Decimal
oreex.plot_hline(Decimal(epe[0]), 'g', "BaselEPE")

eepe = oreex.get_output_data_from_column("swapflat/xva.csv", 21, 1)
oreex.plot_hline(Decimal(eepe[0]), 'y', "BaselEEPE")

oreex.decorate_plot(title="Basel Measures")
oreex.save_plot_to_file()

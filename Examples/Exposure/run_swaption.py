#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposure: Swaptions                              |")
print("+--------------------------------------------------+")

# Legacy examples 3, 4 and 5 

oreex.run("Input/ore_swaption.xml")

oreex.setup_plot("exposure_european_swaption")
oreex.plot("swaption/exposure_trade_ForwardSwap.csv", 2, 3, 'b', "EPE Forward Swap")
oreex.plot("swaption/exposure_trade_EuropeanSwaptionCash.csv", 2, 3, 'r', "EPE European Swaption Cash")
oreex.plot("swaption/exposure_trade_EuropeanSwaptionPhysical.csv", 2, 3, 'g', "EPE European Swaption Physical")
oreex.plot("swaption/exposure_trade_EuropeanSwaptionCashPremium.csv", 2, 3, 'c', "EPE European Swaption Cash with Premium")
oreex.decorate_plot(title="European Swaption Exposures")
oreex.save_plot_to_file()

oreex.setup_plot("exposure_bermudan_swaption")
oreex.plot("swaption/exposure_trade_ForwardSwap.csv", 2, 3, 'b', "EPE Forward Swap")
oreex.plot("swaption/exposure_trade_BermudanSwaptionCash.csv", 2, 3, 'r', "EPE Bermudan Swaption (Cash)")
oreex.plot("swaption/exposure_trade_AmericanSwaptionCash.csv", 2, 3, 'm', "EPE American Swaption (Cash)")
oreex.plot("swaption/exposure_trade_BermudanSwaptionPhysical.csv", 2, 3, 'g', "EPE Bermudan Swaption (Physical)")
oreex.plot("swaption/exposure_trade_AmericanSwaptionPhysical.csv", 2, 3, 'y', "EPE American Swaption (Physical)")
oreex.decorate_plot(title="Bermudan/American Swaption Exposure", ylabel="Exposure")
oreex.save_plot_to_file()

oreex.setup_plot("exposure_callableswap")
oreex.plot("swaption/exposure_trade_Swap.csv", 2, 3, 'b', "EPE Swap")
oreex.plot("swaption/exposure_trade_Swaption.csv", 2, 4, 'r', "ENE Swaption")
oreex.plot("swaption/exposure_nettingset_CPTY_B.csv", 2, 3, 'g', "EPE Netting Set")
oreex.plot("swaption/exposure_trade_ShortSwap.csv", 2, 3, 'm', "EPE Short Swap")
oreex.plot("swaption/exposure_trade_CallableSwap.csv", 2, 3, 'y', "EPE Callable Swap")
oreex.decorate_plot(title="Callable Swap Exposure")
oreex.save_plot_to_file()

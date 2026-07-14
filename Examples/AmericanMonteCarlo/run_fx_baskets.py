#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv) > 1 else False)

print("+-------------------------------------------------------------------+")
print("| AMC: FX Basket Trades (BasketVarianceSwap, WorstOfBasketSwap)|")
print("+-------------------------------------------------------------------+")

# Run ORE with AMC simulation for the two FX basket trade types:
#   - FxBasketVarianceSwap
#     EUR-USD / EUR-GBP basket, 2-year daily vol-swap, EUR payment
#   - FxWorstOfBasketSwap
#     EUR-USD / EUR-GBP basket, 3-year semi-annual EUR-EURIBOR-3M swap

oreex.print_headline("Run ORE AMC exposure for FX basket scripted trades")
oreex.run("Input/ore_fx_baskets_amc.xml")

oreex.print_headline("Plot: FxBasketVarianceSwap exposure (AMC EPE/ENE)")
oreex.setup_plot("BVS_FX")
oreex.plot("fx_baskets_amc/exposure_trade_SCRIPTED_BVS_FX.csv", 2, 3, 'b', "AMC EPE", linestyle='-')
oreex.plot("fx_baskets_amc/exposure_trade_SCRIPTED_BVS_FX.csv", 2, 4, 'r', "AMC ENE", linestyle='-')
oreex.decorate_plot(title="AMC Exposure - FxBasketVarianceSwap (EUR-USD / EUR-GBP, 2Y vol-swap)")
oreex.save_plot_to_file()

oreex.print_headline("Plot: FxWorstOfBasketSwap exposure (AMC EPE/ENE)")
oreex.setup_plot("WOBS_FX")
oreex.plot("fx_baskets_amc/exposure_trade_SCRIPTED_WOBS_FX.csv", 2, 3, 'b', "AMC EPE", linestyle='-')
oreex.plot("fx_baskets_amc/exposure_trade_SCRIPTED_WOBS_FX.csv", 2, 4, 'r', "AMC ENE", linestyle='-')
oreex.decorate_plot(title="AMC Exposure - FxWorstOfBasketSwap (EUR-USD / EUR-GBP, 3Y Euribor swap)")
oreex.save_plot_to_file()

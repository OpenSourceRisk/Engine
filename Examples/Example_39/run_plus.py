#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE+ to produce AMC exposure")
oreex.run_plus("Input/ore_plus.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot results: Simulated exposure")

oreex.setup_plot("bermudanswaption")
oreex.plot("exposure_trade_BermSwp.csv", 2, 3, 'b', "Swaption EPE")
oreex.plot("exposure_trade_BermSwp.csv", 2, 4, 'r', "Swaption ENE")
oreex.decorate_plot(title="Example AMC - Simulated exposure for 10y10y EUR Bermudan Payer Swaption")
oreex.save_plot_to_file()

oreex.setup_plot("vanillaswap_eur")
oreex.plot("exposure_trade_Swap_EUR.csv", 2, 3, 'b', "Vanilla Swap EUR EPE")
oreex.plot("exposure_trade_Swap_EUR.csv", 2, 4, 'r', "Vanilla Swap EUR ENE")
oreex.decorate_plot(title="Example AMC - Simulated exposure for 20y EUR Payer Swap")
oreex.save_plot_to_file()

oreex.setup_plot("vanillaswap_usd")
oreex.plot("exposure_trade_Swap_USD.csv", 2, 3, 'b', "Vanilla Swap USD EPE")
oreex.plot("exposure_trade_Swap_USD.csv", 2, 4, 'r', "Vanilla Swap USD ENE")
oreex.decorate_plot(title="Example AMC - Simulated exposure for 20y EUR Payer Swap")
oreex.save_plot_to_file()

oreex.setup_plot("fxoption")
oreex.plot("exposure_trade_FX_CALL_OPTION.csv", 2, 3, 'b', "FX Call Option EPE")
oreex.plot("exposure_trade_FX_CALL_OPTION.csv", 2, 4, 'r', "FX Call Option ENE")
oreex.decorate_plot(title="Example AMC - Simulated exposure for FX Call Option EUR-USD")
oreex.save_plot_to_file()

oreex.setup_plot("xccyswap")
oreex.plot("exposure_trade_CC_SWAP_EUR_USD.csv", 2, 3, 'b', "XCcy Swap EPE")
oreex.plot("exposure_trade_CC_SWAP_EUR_USD.csv", 2, 4, 'r', "XCcy Swap ENE")
oreex.decorate_plot(title="Example AMC - Simulated exposure for XCcy Swap EUR-USD")
oreex.save_plot_to_file()

oreex.setup_plot("nettingset")
oreex.plot("exposure_nettingset_CPTY_A.csv", 2, 3, 'b', "Nettingset EPE")
oreex.plot("exposure_nettingset_CPTY_A.csv", 2, 4, 'r', "Nettingset ENE")
oreex.decorate_plot(title="Example AMC - Simulated exposure for nettingset")
oreex.save_plot_to_file()

#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce classic exposures")
oreex.run("Input/ore_classic.xml")

oreex.print_headline("Run ORE to produce AMC exposure")
oreex.run("Input/ore_amc.xml")

oreex.print_headline("Run ORE to produce AMC-CG exposure")
oreex.run("Input/ore_amccg.xml")

oreex.print_headline("Plot results: Simulated exposure")

oreex.setup_plot("amc_bermudanswaption")
oreex.plot("amc/exposure_trade_BermSwp.csv", 2, 3, 'b', "AMC Swaption EPE")
oreex.plot("amc/exposure_trade_BermSwp.csv", 2, 4, 'r', "AMC Swaption ENE")
oreex.plot("classic/exposure_trade_BermSwp.csv", 2, 3, 'g', "Classic Swaption EPE")
oreex.plot("classic/exposure_trade_BermSwp.csv", 2, 4, 'y', "Classic Swaption ENE")
oreex.decorate_plot(title="Example 39 - Simulated exposure for 10y10y EUR Bermudan Payer Swaption")
oreex.save_plot_to_file()

oreex.setup_plot("amc_vanillaswap_eur")
oreex.plot("amc/exposure_trade_Swap_EUR.csv", 2, 3, 'b', "AMC Vanilla Swap EUR EPE")
oreex.plot("amc/exposure_trade_Swap_EUR.csv", 2, 4, 'r', "AMC Vanilla Swap EUR ENE")
oreex.plot("classic/exposure_trade_Swap_EUR.csv", 2, 3, 'g', "Classic Vanilla Swap EUR EPE")
oreex.plot("classic/exposure_trade_Swap_EUR.csv", 2, 4, 'y', "Classic Vanilla Swap EUR ENE")
oreex.decorate_plot(title="Example 39 - Simulated exposure for 20y EUR Payer Swap")
oreex.save_plot_to_file()

oreex.setup_plot("amc_vanillaswap_usd")
oreex.plot("amc/exposure_trade_Swap_USD.csv", 2, 3, 'b', "AMC Vanilla Swap USD EPE")
oreex.plot("amc/exposure_trade_Swap_USD.csv", 2, 4, 'r', "AMC Vanilla Swap USD ENE")
oreex.plot("classic/exposure_trade_Swap_USD.csv", 2, 3, 'g', "Classic Vanilla Swap USD EPE")
oreex.plot("classic/exposure_trade_Swap_USD.csv", 2, 4, 'y', "Classic Vanilla Swap USD ENE")
oreex.decorate_plot(title="Example 39 - Simulated exposure for 20y EUR Payer Swap")
oreex.save_plot_to_file()

oreex.setup_plot("amc_fxoption")
oreex.plot("amc/exposure_trade_FX_CALL_OPTION.csv", 2, 3, 'b', "AMC FX Call Option EPE")
oreex.plot("amc/exposure_trade_FX_CALL_OPTION.csv", 2, 4, 'r', "AMC FX Call Option ENE")
oreex.plot("classic/exposure_trade_FX_CALL_OPTION.csv", 2, 3, 'g', "Classic FX Call Option EPE")
oreex.plot("classic/exposure_trade_FX_CALL_OPTION.csv", 2, 4, 'y', "Classic FX Call Option ENE")
oreex.decorate_plot(title="Example 39 - Simulated exposure for FX Call Option EUR-USD")
oreex.save_plot_to_file()

oreex.setup_plot("amc_xccyswap")
oreex.plot("amc/exposure_trade_CC_SWAP_EUR_USD.csv", 2, 3, 'b', "AMC XCcy Swap EPE")
oreex.plot("amc/exposure_trade_CC_SWAP_EUR_USD.csv", 2, 4, 'r', "AMC XCcy Swap ENE")
oreex.plot("classic/exposure_trade_CC_SWAP_EUR_USD.csv", 2, 3, 'g', "Classic XCcy Swap EPE")
oreex.plot("classic/exposure_trade_CC_SWAP_EUR_USD.csv", 2, 4, 'y', "Classic XCcy Swap ENE")
oreex.decorate_plot(title="Example 39 - Simulated exposure for XCcy Swap EUR-USD")
oreex.save_plot_to_file()

oreex.setup_plot("amc_nettingset")
oreex.plot("amc/exposure_nettingset_CPTY_A.csv", 2, 3, 'b', "AMC Netting Set EPE")
oreex.plot("amc/exposure_nettingset_CPTY_A.csv", 2, 4, 'r', "AMC Netting Set ENE")
oreex.plot("classic/exposure_nettingset_CPTY_A.csv", 2, 3, 'g', "Classic Netting Set EPE")
oreex.plot("classic/exposure_nettingset_CPTY_A.csv", 2, 4, 'y', "Classic Netting Set ENE")
oreex.decorate_plot(title="Example 39 - Simulated exposure for the netting set")
oreex.save_plot_to_file()

# plots for amc-cg instead of amc

oreex.setup_plot("amc-cg_bermudanswaption")
oreex.plot("amccg/exposure_trade_BermSwp.csv", 2, 3, 'b', "AMC-CG Swaption EPE")
oreex.plot("amccg/exposure_trade_BermSwp.csv", 2, 4, 'r', "AMC-CG Swaption ENE")
oreex.plot("classic/exposure_trade_BermSwp.csv", 2, 3, 'g', "Classic Swaption EPE")
oreex.plot("classic/exposure_trade_BermSwp.csv", 2, 4, 'y', "Classic Swaption ENE")
oreex.decorate_plot(title="Example 39 - Simulated exposure for 10y10y EUR Bermudan Payer Swaption")
oreex.save_plot_to_file()

oreex.setup_plot("amc-cg_vanillaswap_eur")
oreex.plot("amccg/exposure_trade_Swap_EUR.csv", 2, 3, 'b', "AMC-CG Vanilla Swap EUR EPE")
oreex.plot("amccg/exposure_trade_Swap_EUR.csv", 2, 4, 'r', "AMC-CG Vanilla Swap EUR ENE")
oreex.plot("classic/exposure_trade_Swap_EUR.csv", 2, 3, 'g', "Classic Vanilla Swap EUR EPE")
oreex.plot("classic/exposure_trade_Swap_EUR.csv", 2, 4, 'y', "Classic Vanilla Swap EUR ENE")
oreex.decorate_plot(title="Example 39 - Simulated exposure for 20y EUR Payer Swap")
oreex.save_plot_to_file()

oreex.setup_plot("amc-cg_vanillaswap_usd")
oreex.plot("amccg/exposure_trade_Swap_USD.csv", 2, 3, 'b', "AMC-CG Vanilla Swap USD EPE")
oreex.plot("amccg/exposure_trade_Swap_USD.csv", 2, 4, 'r', "AMC-CG Vanilla Swap USD ENE")
oreex.plot("classic/exposure_trade_Swap_USD.csv", 2, 3, 'g', "Classic Vanilla Swap USD EPE")
oreex.plot("classic/exposure_trade_Swap_USD.csv", 2, 4, 'y', "Classic Vanilla Swap USD ENE")
oreex.decorate_plot(title="Example 39 - Simulated exposure for 20y EUR Payer Swap")
oreex.save_plot_to_file()

oreex.setup_plot("amc-cg_fxoption")
oreex.plot("amccg/exposure_trade_FX_CALL_OPTION.csv", 2, 3, 'b', "AMC-CG FX Call Option EPE")
oreex.plot("amccg/exposure_trade_FX_CALL_OPTION.csv", 2, 4, 'r', "AMC-CG FX Call Option ENE")
oreex.plot("classic/exposure_trade_FX_CALL_OPTION.csv", 2, 3, 'g', "Classic FX Call Option EPE")
oreex.plot("classic/exposure_trade_FX_CALL_OPTION.csv", 2, 4, 'y', "Classic FX Call Option ENE")
oreex.decorate_plot(title="Example 39 - Simulated exposure for FX Call Option EUR-USD")
oreex.save_plot_to_file()

oreex.setup_plot("amc-cg_xccyswap")
oreex.plot("amccg/exposure_trade_CC_SWAP_EUR_USD.csv", 2, 3, 'b', "AMC-CG XCcy Swap EPE")
oreex.plot("amccg/exposure_trade_CC_SWAP_EUR_USD.csv", 2, 4, 'r', "AMC-CG XCcy Swap ENE")
oreex.plot("classic/exposure_trade_CC_SWAP_EUR_USD.csv", 2, 3, 'g', "Classic XCcy Swap EPE")
oreex.plot("classic/exposure_trade_CC_SWAP_EUR_USD.csv", 2, 4, 'y', "Classic XCcy Swap ENE")
oreex.decorate_plot(title="Example 39 - Simulated exposure for XCcy Swap EUR-USD")
oreex.save_plot_to_file()

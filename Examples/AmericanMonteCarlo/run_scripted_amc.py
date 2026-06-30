#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv) > 1 else False)

print("+----------------------------------------------------------------------+")
print("| AMC: Scripted Trade Types (Barrier, Accumulator, KnockOutSwap, etc.) |")
print("+----------------------------------------------------------------------+")

# Run ORE with AMC simulation for all scripted trade types:
#   EuropeanOptionBarrier (European style)    Pattern 1 – single regression step
#   EuropeanOptionBarrier (American style)    Pattern 1 – monitored barrier
#   FxWindowBarrierOption                     Pattern 1 – windowed monitoring
#   FxBestEntryOption                         Pattern 2 – path-dependent entry reset
#   FxStrikeResettableOption                  Pattern 2 – conditional strike reset
#   FxAccumulator type 01                     Pattern 2 – monthly fixings, European KO
#   FxAccumulator type 02                     Pattern 2 – weekly obs, monthly periods, KO
#   KnockOutSwap                              Pattern 2 – quarterly USD SOFR swap, KO

oreex.print_headline("Run ORE AMC exposure for scripted trade types")
oreex.run("Input/ore_scripted.xml")

oreex.print_headline("Plot: EuropeanOptionBarrier (European style) exposure (AMC)")
oreex.setup_plot("EOB_EUR_FX")
oreex.plot("scripted_trades/exposure_trade_SCRIPTED_EOB_EUR_FX.csv", 2, 3, 'b', "AMC EPE", linestyle='-')
oreex.plot("scripted_trades/exposure_trade_SCRIPTED_EOB_EUR_FX.csv", 2, 4, 'r', "AMC ENE", linestyle='-')
oreex.decorate_plot(title="AMC Exposure - EuropeanOptionBarrier (EUR-USD, European KO at 0.98, 1Y)")
oreex.save_plot_to_file()

oreex.print_headline("Plot: EuropeanOptionBarrier (American style) exposure (AMC)")
oreex.setup_plot("EOB_AMC_FX")
oreex.plot("scripted_trades/exposure_trade_SCRIPTED_EOB_AMC_FX.csv", 2, 3, 'b', "AMC EPE", linestyle='-')
oreex.plot("scripted_trades/exposure_trade_SCRIPTED_EOB_AMC_FX.csv", 2, 4, 'r', "AMC ENE", linestyle='-')
oreex.decorate_plot(title="AMC Exposure - EuropeanOptionBarrier (EUR-USD, American KO at 0.98 monthly, 1Y)")
oreex.save_plot_to_file()

oreex.print_headline("Plot: FxWindowBarrierOption exposure (AMC)")
oreex.setup_plot("WBO_FX")
oreex.plot("scripted_trades/exposure_trade_SCRIPTED_WBO_FX.csv", 2, 3, 'b', "AMC EPE", linestyle='-')
oreex.plot("scripted_trades/exposure_trade_SCRIPTED_WBO_FX.csv", 2, 4, 'r', "AMC ENE", linestyle='-')
oreex.decorate_plot(title="AMC Exposure - FxWindowBarrierOption (EUR-USD, DownAndOut 6M window, 1Y)")
oreex.save_plot_to_file()

oreex.print_headline("Plot: FxBestEntryOption exposure (AMC)")
oreex.setup_plot("BEO_FX")
oreex.plot("scripted_trades/exposure_trade_SCRIPTED_BEO_FX.csv", 2, 3, 'b', "AMC EPE", linestyle='-')
oreex.plot("scripted_trades/exposure_trade_SCRIPTED_BEO_FX.csv", 2, 4, 'r', "AMC ENE", linestyle='-')
oreex.decorate_plot(title="AMC Exposure - FxBestEntryOption (EUR-USD participation, 6M obs, 1Y)")
oreex.save_plot_to_file()

oreex.print_headline("Plot: FxStrikeResettableOption exposure (AMC)")
oreex.setup_plot("SRO_FX")
oreex.plot("scripted_trades/exposure_trade_SCRIPTED_SRO_FX.csv", 2, 3, 'b', "AMC EPE", linestyle='-')
oreex.plot("scripted_trades/exposure_trade_SCRIPTED_SRO_FX.csv", 2, 4, 'r', "AMC ENE", linestyle='-')
oreex.decorate_plot(title="AMC Exposure - FxStrikeResettableOption (EUR-USD, 3 quarterly resets, 1Y)")
oreex.save_plot_to_file()

oreex.print_headline("Plot: FxAccumulator type 01 exposure (AMC)")
oreex.setup_plot("ACC01_FX")
oreex.plot("scripted_trades/exposure_trade_SCRIPTED_ACC01_FX.csv", 2, 3, 'b', "AMC EPE", linestyle='-')
oreex.plot("scripted_trades/exposure_trade_SCRIPTED_ACC01_FX.csv", 2, 4, 'r', "AMC ENE", linestyle='-')
oreex.decorate_plot(title="AMC Exposure - FxAccumulator type 01 (EUR-USD, monthly, European KO, 1Y)")
oreex.save_plot_to_file()

oreex.print_headline("Plot: FxAccumulator type 02 exposure (AMC)")
oreex.setup_plot("ACC02_FX")
oreex.plot("scripted_trades/exposure_trade_SCRIPTED_ACC02_FX.csv", 2, 3, 'b', "AMC EPE", linestyle='-')
oreex.plot("scripted_trades/exposure_trade_SCRIPTED_ACC02_FX.csv", 2, 4, 'r', "AMC ENE", linestyle='-')
oreex.decorate_plot(title="AMC Exposure - FxAccumulator type 02 (EUR-USD, weekly obs, monthly periods, 6M)")
oreex.save_plot_to_file()

oreex.print_headline("Plot: KnockOutSwap exposure (AMC)")
oreex.setup_plot("KOS_USD")
oreex.plot("scripted_trades/exposure_trade_SCRIPTED_KOS_USD.csv", 2, 3, 'b', "AMC EPE", linestyle='-')
oreex.plot("scripted_trades/exposure_trade_SCRIPTED_KOS_USD.csv", 2, 4, 'r', "AMC ENE", linestyle='-')
oreex.decorate_plot(title="AMC Exposure - KnockOutSwap (USD SOFR 3Y, pay 4.5%, KO at 5.5%)")
oreex.save_plot_to_file()

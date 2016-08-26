#!/usr/bin/env python

import glob

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

import sys
oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

# whithout collateral
oreex.print_headline("Run ORE to produce NPV cube and exposures, without collateral")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")
oreex.save_output_to_subdir(
    "collateral_none",
    ["log.txt", "xva.csv"] + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
)

oreex.print_headline("Plot results")

oreex.setup_plot("nocollateral_epe")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_1.csv"), 2, 3, 'b', "EPE Swap 1")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_2.csv"), 2, 3, 'r', "EPE Swap 2")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_3.csv"), 2, 3, 'g', "EPE Swap 3")
oreex.plot(os.path.join("collateral_none", "exposure_nettingset_CPTY_A.csv"), 2, 3, 'm', "EPE NettingSet")
oreex.decorate_plot(title="Example 5")
oreex.save_plot_to_file()


#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from oreplus_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

# whithout VM/IM
oreex.print_headline("Run ORE to produce NPV cube and exposures, without VM/IM")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")
oreex.save_output_to_subdir(
    "collateral_none",
    ["log.txt", "xva.csv"] + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
)

# with VM, threshhold=mta=0, mpor=2w
oreex.print_headline("Run ORE to postprocess the NPV cube, with VM, without IM")
oreex.run("Input/ore_mpor.xml")
oreex.save_output_to_subdir(
    "collateral_mpor",
    ["log.txt", "xva.csv"]
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "colva*")))
)

# with VM and IM, threshold=mta=0, mpor=2w
oreex.print_headline("Run ORE to postprocess the NPV cube, with VM and IM")
oreex.run("Input/ore_mpor_dim.xml")
oreex.save_output_to_subdir(
    "collateral_mpor_dim",
    ["log.txt", "xva.csv"]
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "colva*")))
)

oreex.print_headline("Plot results")

oreex.setup_plot("nocollateral_epe")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_1.csv"), 2, 3, 'b', "EPE Swap 1")
oreex.plot(os.path.join("collateral_none", "exposure_nettingset_CPTY_A.csv"), 2, 3, 'm', "EPE NettingSet")
oreex.decorate_plot(title="Example 10")
oreex.save_plot_to_file()

oreex.setup_plot("nocollateral_ene")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_1.csv"), 2, 4, 'b', "ENE Swap 1")
oreex.plot(os.path.join("collateral_none", "exposure_nettingset_CPTY_A.csv"), 2, 4, 'm', "ENE NettingSet")
oreex.decorate_plot(title="Example 10")
oreex.save_plot_to_file()

oreex.setup_plot("mpor_epe")
oreex.plot(os.path.join("collateral_none","exposure_nettingset_CPTY_A.csv"), 2, 3, 'b', "EPE NettingSet")
oreex.plot(os.path.join("collateral_mpor","exposure_nettingset_CPTY_A.csv"), 2, 3, 'r', "EPE NettingSet, MPOR 2W")
oreex.plot(os.path.join("collateral_mpor_dim","exposure_nettingset_CPTY_A.csv"), 2, 3, 'g', "EPE NettingSet, MPOR 2W, DIM")
oreex.decorate_plot(title="Example 10")
oreex.save_plot_to_file()

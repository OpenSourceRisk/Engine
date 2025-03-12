#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

# whithout collateral
oreex.print_headline("Run ORE to produce NPV cube and exposures, without collateral")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")
oreex.save_output_to_subdir(
    "collateral_none",
    ["log.txt", "xva.csv"]
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
)

# threshhold=mta=0
oreex.print_headline("Run ORE to postprocess the NPV cube, with VM (threshold=mta=0)")
oreex.run("Input/ore_mpor.xml")
oreex.save_output_to_subdir(
    "collateral_mpor",
    ["log.txt", "xva.csv"]
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "colva*")))
)

# threshhold=mta=0 + regression dim
oreex.print_headline("Run ORE to postprocess the NPV cube, with VM (threshold=mta=0) and Regression IM")
oreex.run("Input/ore_dim.xml")
oreex.save_output_to_subdir(
    "collateral_dim",
    ["log.txt", "xva.csv"]
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "colva*")))
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "dim_evolution*")))
)

# threshhold=mta=0 + deltavar dim
oreex.print_headline("Run ORE to postprocess the NPV cube, with VM (threshold=mta=0) and DeltaVaR IM")
oreex.run("Input/ore_ddv.xml")
oreex.save_output_to_subdir(
    "collateral_ddv",
    ["log.txt", "xva.csv"]
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "colva*")))
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "dim_evolution*")))
)

oreex.print_headline("Plot results")

oreex.setup_plot("nocollateral_epe")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_1.csv"), 2, 3, 'b', "EPE Swap 1")
oreex.plot(os.path.join("collateral_none", "exposure_nettingset_CPTY_A.csv"), 2, 3, 'm', "EPE NettingSet")
oreex.decorate_plot(title="Example 31")
oreex.save_plot_to_file()

oreex.setup_plot("nocollateral_ene")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_1.csv"), 2, 4, 'b', "ENE Swap 1")
oreex.plot(os.path.join("collateral_none", "exposure_nettingset_CPTY_A.csv"), 2, 4, 'm', "ENE NettingSet")
oreex.decorate_plot(title="Example 31")
oreex.save_plot_to_file()

oreex.setup_plot("mpor_epe")
oreex.plot(os.path.join("collateral_none","exposure_nettingset_CPTY_A.csv"), 2, 3, 'b', "EPE NettingSet")
oreex.plot(os.path.join("collateral_mpor","exposure_nettingset_CPTY_A.csv"), 2, 3, 'r', "EPE NettingSet, MPOR 2W")
oreex.decorate_plot(title="Example 31")
oreex.save_plot_to_file()

oreex.setup_plot("dim_epe")
#oreex.plot(os.path.join("collateral_none","exposure_nettingset_CPTY_A.csv"), 2, 3, 'b', "EPE NettingSet")
oreex.plot(os.path.join("collateral_mpor","exposure_nettingset_CPTY_A.csv"), 2, 3, 'r', "EPE NettingSet, MPOR 2W")
oreex.plot(os.path.join("collateral_dim","exposure_nettingset_CPTY_A.csv"), 2, 3, 'g', "EPE NettingSet, MPOR 2W and Regression DIM")
oreex.decorate_plot(title="Example 31")
oreex.save_plot_to_file()

#oreex.setup_plot("dim_epe_ddv")
#oreex.plot(os.path.join("collateral_none","exposure_nettingset_CPTY_A.csv"), 2, 3, 'b', "EPE NettingSet")
#oreex.plot(os.path.join("collateral_mpor","exposure_nettingset_CPTY_A.csv"), 2, 3, 'r', "EPE NettingSet, MPOR 2W")
#oreex.plot(os.path.join("collateral_ddv","exposure_nettingset_CPTY_A.csv"), 2, 3, 'g', "EPE NettingSet, MPOR 2W and DeltaVaR DIM")
#oreex.decorate_plot(title="Example 31")
#oreex.save_plot_to_file()

oreex.setup_plot("dim_evolution")
#oreex.plot(os.path.join("collateral_dim", "dim_evolution.csv"), 0, 6, 'y', "Simple")
oreex.plot(os.path.join("collateral_dim", "dim_evolution.csv"), 0, 4, 'c', "Regression")
oreex.plot(os.path.join("collateral_ddv", "dim_evolution.csv"), 0, 3, 'm', "Delta VaR")
oreex.decorate_plot(title="Example 31 - DIM Evolution", xlabel="Timestep", ylabel="DIM", legend_loc="lower left")
oreex.save_plot_to_file()

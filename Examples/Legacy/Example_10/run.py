#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

# Run quick example with 0 IAH
oreex.print_headline("Run ORE to produce NPV cube and exposures, with IAH=0")
oreex.run("Input/ore_iah_0.xml")

# Re-run with IAH=1m
oreex.print_headline("Run ORE to postprocess the NPV cube, with IAH=1m")
oreex.run("Input/ore_iah_1.xml")

# Plot IAH results 
oreex.print_headline("Plot IAH results")
oreex.setup_plot("Exposures_IAH")
oreex.plot("collateral_iah_0/exposure_nettingset_CPTY_A.csv", 2, 3, 'b', "EPE IAH=0")
oreex.plot("collateral_iah_1/exposure_nettingset_CPTY_A.csv", 2, 3, 'g', "EPE IAH=1m")
oreex.plot("collateral_iah_0/exposure_nettingset_CPTY_A.csv", 2, 5, 'r', "PFE IAH=0")
oreex.plot("collateral_iah_1/exposure_nettingset_CPTY_A.csv", 2, 5, 'y', "PFE IAH=1m")
oreex.decorate_plot(title="IAH Example")
oreex.save_plot_to_file()

# Run time-consuming example whithout collateral
oreex.print_headline("Run ORE to produce NPV cube and exposures, without collateral")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")
oreex.save_output_to_subdir(
    "collateral_none",
    ["log.txt", "xva.csv"] + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
)

# Threshhold>0
oreex.print_headline("Run ORE to postprocess the NPV cube, with collateral (threshold>0)")
oreex.run("Input/ore_threshold.xml")
oreex.save_output_to_subdir(
    "collateral_threshold",
    ["log.txt", "xva.csv"]
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "colva*")))
)

# mta=0
oreex.print_headline("Run ORE to postprocess the NPV cube, with collateral (threshold=0)")
oreex.run("Input/ore_mta.xml")
oreex.save_output_to_subdir(
    "collateral_mta",
    ["log.txt", "xva.csv"]
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "colva*")))
)

# threshhold=mta=0
oreex.print_headline("Run ORE to postprocess the NPV cube, with collateral (threshold=mta=0)")
oreex.run("Input/ore_mpor.xml")
oreex.save_output_to_subdir(
    "collateral_mpor",
    ["log.txt", "xva.csv"]
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "colva*")))
)


# threshhold>0 with collateral
oreex.print_headline("Run ORE to postprocess the NPV cube, with collateral (threshold>0), exercise next break")
oreex.run("Input/ore_threshold_break.xml")
oreex.save_output_to_subdir(
    "collateral_threshold_break",
    ["log.txt", "xva.csv"]
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "colva*")))
)

# threshhold>0 with collateral and dynamic initial margin
oreex.print_headline("Run ORE to postprocess the NPV cube, with collateral (threshold>0) and dynamic initial margin")
oreex.run("Input/ore_threshold_dim.xml")
oreex.save_output_to_subdir(
    "collateral_threshold_dim",
    ["log.txt", "xva.csv", "dim_regression.csv", "dim_evolution.csv"]
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "colva*")))
)

oreex.print_headline("Plot results")

oreex.setup_plot("nocollateral_epe")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_1.csv"), 2, 3, 'b', "EPE Swap 1")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_2.csv"), 2, 3, 'r', "EPE Swap 2")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_3.csv"), 2, 3, 'g', "EPE Swap 3")
oreex.plot(os.path.join("collateral_none", "exposure_nettingset_CPTY_A.csv"), 2, 3, 'm', "EPE NettingSet")
oreex.decorate_plot(title="Example 10")
oreex.save_plot_to_file()

oreex.setup_plot("nocollateral_ene")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_1.csv"), 2, 4, 'b', "ENE Swap 1")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_2.csv"), 2, 4, 'r', "ENE Swap 2")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_3.csv"), 2, 4, 'g', "ENE Swap 3")
oreex.plot(os.path.join("collateral_none", "exposure_nettingset_CPTY_A.csv"), 2, 4, 'm', "ENE NettingSet")
oreex.decorate_plot(title="Example 10")
oreex.save_plot_to_file()

oreex.setup_plot("nocollateral_allocated_epe")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_1.csv"), 2, 5, 'b', "Allocated EPE Swap 1")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_2.csv"), 2, 5, 'r', "Allocated EPE Swap 2")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_3.csv"), 2, 5, 'g', "Allocated EPE Swap 3")
oreex.decorate_plot(title="Example 10")
oreex.save_plot_to_file()

oreex.setup_plot("nocollateral_allocated_ene")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_1.csv"), 2, 6, 'b', "Allocated ENE Swap 1")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_2.csv"), 2, 6, 'r', "Allocated ENE Swap 2")
oreex.plot(os.path.join("collateral_none", "exposure_trade_Swap_3.csv"), 2, 6, 'g', "Allocated ENE Swap 3")
oreex.decorate_plot(title="Example 10")
oreex.save_plot_to_file()

oreex.setup_plot("threshold_epe")
oreex.plot(os.path.join("collateral_none", "exposure_nettingset_CPTY_A.csv"), 2, 3, 'b', "EPE NettingSet")
oreex.plot(os.path.join("collateral_threshold","exposure_nettingset_CPTY_A.csv"), 2, 3, 'r', "EPE NettingSet, Threshold 1m")
oreex.decorate_plot(title="Example 10")
oreex.save_plot_to_file()

oreex.setup_plot("threshold_allocated_epe")
oreex.plot(os.path.join("collateral_threshold","exposure_trade_Swap_1.csv"), 2, 5, 'b', "Allocated EPE Swap 1")
oreex.plot(os.path.join("collateral_threshold","exposure_trade_Swap_2.csv"), 2, 5, 'r', "Allocated EPE Swap 2")
oreex.plot(os.path.join("collateral_threshold","exposure_trade_Swap_3.csv"), 2, 5, 'g', "Allocated EPE Swap 3")
oreex.decorate_plot(title="Example 10")
oreex.save_plot_to_file()

oreex.setup_plot("mta_epe")
oreex.plot(os.path.join("collateral_none","exposure_nettingset_CPTY_A.csv"), 2, 3, 'b', "EPE NettingSet")
oreex.plot(os.path.join("collateral_mta","exposure_nettingset_CPTY_A.csv"), 2, 3, 'r', "EPE NettingSet, MTA 100k")
oreex.decorate_plot(title="Example 10")
oreex.save_plot_to_file()

oreex.setup_plot("mpor_epe")
oreex.plot(os.path.join("collateral_none","exposure_nettingset_CPTY_A.csv"), 2, 3, 'b', "EPE NettingSet")
oreex.plot(os.path.join("collateral_mpor","exposure_nettingset_CPTY_A.csv"), 2, 3, 'r', "EPE NettingSet, MPOR 2W")
oreex.decorate_plot(title="Example 10")
oreex.save_plot_to_file()

oreex.setup_plot("collateral")
oreex.plot(os.path.join("collateral_threshold","colva_nettingset_CPTY_A.csv"), 2, 3, 'b', "Collateral Balance", 2)
oreex.decorate_plot(title="Example 10", ylabel="Collateral Balance")
oreex.save_plot_to_file()

oreex.setup_plot("colva")
oreex.plot(os.path.join("collateral_threshold","colva_nettingset_CPTY_A.csv"), 2, 4, 'b', "COLVA Increments", 2)
oreex.decorate_plot(title="Example 10", ylabel="COLVA")
oreex.save_plot_to_file()

oreex.setup_plot("collateral_floor")
oreex.plot(os.path.join("collateral_threshold","colva_nettingset_CPTY_A.csv"), 2, 6, 'b', "Collateral Floor Increments", 2)
oreex.decorate_plot(title="Example 10", ylabel="Collateral Floor Increments")
oreex.save_plot_to_file()

oreex.setup_plot("threshold_break_epe")
oreex.plot(os.path.join("collateral_threshold","exposure_nettingset_CPTY_A.csv"), 2, 3, 'b', "EPE NettingSet, Threshold 1m")
oreex.plot(os.path.join("collateral_threshold_break","exposure_nettingset_CPTY_A.csv"), 2, 3, 'r', "EPE NettingSet, Threshold 1m, Breaks")
oreex.decorate_plot(title="Example 10")
oreex.save_plot_to_file()

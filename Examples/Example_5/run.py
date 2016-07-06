#!/usr/bin/env python

import glob

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

oreex = OreExample()

# whithout collateral
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

# plot everything

oreex.setup_plot("nocollateral_epe")
oreex.plot(os.path.join("collateral_none", "exposure_trade_70309.csv"), 2, 3, 'b', "EPE 70309")
oreex.plot(os.path.join("collateral_none", "exposure_trade_919020.csv"), 2, 3, 'r', "EPE 919020")
oreex.plot(os.path.join("collateral_none", "exposure_trade_938498.csv"), 2, 3, 'g', "EPE 938498")
oreex.plot(os.path.join("collateral_none", "exposure_nettingset_CUST_A.csv"), 2, 3, 'm', "EPE NettingSet")
oreex.decorate_plot(title="Example 5")
oreex.save_plot_to_file()

oreex.setup_plot("nocollateral_ene")
oreex.plot(os.path.join("collateral_none", "exposure_trade_70309.csv"), 2, 4, 'b', "ENE 70309")
oreex.plot(os.path.join("collateral_none", "exposure_trade_919020.csv"), 2, 4, 'r', "ENE 919020")
oreex.plot(os.path.join("collateral_none", "exposure_trade_938498.csv"), 2, 4, 'g', "ENE 938498")
oreex.plot(os.path.join("collateral_none", "exposure_nettingset_CUST_A.csv"), 2, 4, 'm', "ENE NettingSet")
oreex.decorate_plot(title="Example 5")
oreex.save_plot_to_file()

oreex.setup_plot("nocollateral_allocated_epe")
oreex.plot(os.path.join("collateral_none", "exposure_trade_70309.csv"), 2, 5, 'b', "Allocated EPE 70309")
oreex.plot(os.path.join("collateral_none", "exposure_trade_919020.csv"), 2, 5, 'r', "Allocated EPE 919020")
oreex.plot(os.path.join("collateral_none", "exposure_trade_938498.csv"), 2, 5, 'g', "Allocated EPE 938498")
oreex.decorate_plot(title="Example 5")
oreex.save_plot_to_file()

oreex.setup_plot("nocollateral_allocated_ene")
oreex.plot(os.path.join("collateral_none", "exposure_trade_70309.csv"), 2, 6, 'b', "Allocated ENE 70309")
oreex.plot(os.path.join("collateral_none", "exposure_trade_919020.csv"), 2, 6, 'r', "Allocated ENE 919020")
oreex.plot(os.path.join("collateral_none", "exposure_trade_938498.csv"), 2, 6, 'g', "Allocated ENE 938498")
oreex.decorate_plot(title="Example 5")
oreex.save_plot_to_file()

oreex.setup_plot("threshold_epe")
oreex.plot(os.path.join("collateral_none", "exposure_nettingset_CUST_A.csv"), 2, 3, 'b', "EPE NettingSet")
oreex.plot(os.path.join("collateral_threshold","exposure_nettingset_CUST_A.csv"), 2, 3, 'r', "EPE NettingSet, Threshold 1m")
oreex.decorate_plot(title="Example 5")
oreex.save_plot_to_file()

oreex.setup_plot("threshold_allocated_epe")
oreex.plot(os.path.join("collateral_threshold","exposure_trade_70309.csv"), 2, 5, 'b', "Allocated EPE 70309")
oreex.plot(os.path.join("collateral_threshold","exposure_trade_919020.csv"), 2, 5, 'r', "Allocated EPE 919020")
oreex.plot(os.path.join("collateral_threshold","exposure_trade_938498.csv"), 2, 5, 'g', "Allocated EPE 938498")
oreex.decorate_plot(title="Example 5")
oreex.save_plot_to_file()

oreex.setup_plot("mta_epe")
oreex.plot(os.path.join("collateral_none","exposure_nettingset_CUST_A.csv"), 2, 3, 'b', "EPE NettingSet")
oreex.plot(os.path.join("collateral_mta","exposure_nettingset_CUST_A.csv"), 2, 3, 'r', "EPE NettingSet, MTA 100k")
oreex.decorate_plot(title="Example 5")
oreex.save_plot_to_file()

oreex.setup_plot("mpor_epe")
oreex.plot(os.path.join("collateral_none","exposure_nettingset_CUST_A.csv"), 2, 3, 'b', "EPE NettingSet")
oreex.plot(os.path.join("collateral_mpor","exposure_nettingset_CUST_A.csv"), 2, 3, 'r', "EPE NettingSet, MPOR 2W")
oreex.decorate_plot(title="Example 5")
oreex.save_plot_to_file()

oreex.setup_plot("collateral")
oreex.plot(os.path.join("collateral_threshold","colva_nettingset_CUST_A.csv"), 2, 3, 'b', "Collateral Balance", 2)
oreex.decorate_plot(title="Example 5", ylabel="Collateral Balance")
oreex.save_plot_to_file()

oreex.setup_plot("colva")
oreex.plot(os.path.join("collateral_threshold","colva_nettingset_CUST_A.csv"), 2, 4, 'b', "COLVA Increments", 2)
oreex.decorate_plot(title="Example 5", ylabel="COLVA")
oreex.save_plot_to_file()

oreex.setup_plot("collateral_floor")
oreex.plot(os.path.join("collateral_threshold","colva_nettingset_CUST_A.csv"), 2, 6, 'b', "Collateral Floor Increments", 2)
oreex.decorate_plot(title="Example 5", ylabel="Collateral Floor Increments")
oreex.save_plot_to_file()

oreex.setup_plot("threshold_break_epe")
oreex.plot(os.path.join("collateral_threshold","exposure_nettingset_CUST_A.csv"), 2, 3, 'b', "EPE NettingSet, Threshold 1m")
oreex.plot(os.path.join("collateral_threshold_break","exposure_nettingset_CUST_A.csv"), 2, 3, 'r', "EPE NettingSet, Threshold 1m, Breaks")
oreex.decorate_plot(title="Example 5")
oreex.save_plot_to_file()

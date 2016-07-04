#!/usr/bin/env python

import os
import matplotlib.pyplot as plt
import runpy
import glob

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

# plot_nocollateral_epe.pdf
fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_title("Example 5")
ax.set_xlabel("Time / Years")
ax.set_ylabel("Exposure")
trade1_time = oreex.get_output_data_from_column("collateral_none/exposure_trade_70309.csv", 2)
trade1_epe = oreex.get_output_data_from_column("collateral_none/exposure_trade_70309.csv", 3)
trade2_time = oreex.get_output_data_from_column("collateral_none/exposure_trade_919020.csv", 2)
trade2_epe = oreex.get_output_data_from_column("collateral_none/exposure_trade_919020.csv", 3)
trade3_time = oreex.get_output_data_from_column("collateral_none/exposure_trade_938498.csv", 2)
trade3_epe = oreex.get_output_data_from_column("collateral_none/exposure_trade_938498.csv", 3)
ns_time = oreex.get_output_data_from_column("collateral_none/exposure_nettingset_CUST_A.csv", 2)
ns_epe = oreex.get_output_data_from_column("collateral_none/exposure_nettingset_CUST_A.csv", 3)
ax.plot(trade1_time, trade1_epe, color='b', label="EPE 70309")
ax.plot(trade2_time, trade2_epe, color='r', label="EPE 919020")
ax.plot(trade3_time, trade3_epe, color='g', label="EPE 938498")
ax.plot(ns_time, ns_epe, color='m', label="EPE NettingSet")
ax.legend(loc='upper right', shadow=True)

# plot_nocollateral_ene
fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_title("Example 5")
ax.set_xlabel("Time / Years")
ax.set_ylabel("Exposure")
trade1_time = oreex.get_output_data_from_column("collateral_none/exposure_trade_70309.csv", 2)
trade1_epe = oreex.get_output_data_from_column("collateral_none/exposure_trade_70309.csv", 4)
trade2_time = oreex.get_output_data_from_column("collateral_none/exposure_trade_919020.csv", 2)
trade2_epe = oreex.get_output_data_from_column("collateral_none/exposure_trade_919020.csv", 4)
trade3_time = oreex.get_output_data_from_column("collateral_none/exposure_trade_938498.csv", 2)
trade3_epe = oreex.get_output_data_from_column("collateral_none/exposure_trade_938498.csv", 4)
ns_time = oreex.get_output_data_from_column("collateral_none/exposure_nettingset_CUST_A.csv", 2)
ns_epe = oreex.get_output_data_from_column("collateral_none/exposure_nettingset_CUST_A.csv", 4)
ax.plot(trade1_time, trade1_epe, color='b', label="ENE 70309")
ax.plot(trade2_time, trade2_epe, color='r', label="ENE 919020")
ax.plot(trade3_time, trade3_epe, color='g', label="ENE 938498")
ax.plot(ns_time, ns_epe, color='m', label="ENE NettingSet")
ax.legend(loc='upper right', shadow=True)

# plot_nocollateral_allocated_epe
fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_title("Example 5")
ax.set_xlabel("Time / Years")
ax.set_ylabel("Exposure")
trade1_time = oreex.get_output_data_from_column("collateral_none/exposure_trade_70309.csv", 2)
trade1_epe = oreex.get_output_data_from_column("collateral_none/exposure_trade_70309.csv", 5)
trade2_time = oreex.get_output_data_from_column("collateral_none/exposure_trade_919020.csv", 2)
trade2_epe = oreex.get_output_data_from_column("collateral_none/exposure_trade_919020.csv", 5)
trade3_time = oreex.get_output_data_from_column("collateral_none/exposure_trade_938498.csv", 2)
trade3_epe = oreex.get_output_data_from_column("collateral_none/exposure_trade_938498.csv", 5)
ax.plot(trade1_time, trade1_epe, color='b', label="Allocated EPE 70309")
ax.plot(trade2_time, trade2_epe, color='r', label="Allocated EPE 919020")
ax.plot(trade3_time, trade3_epe, color='g', label="Allocated EPE 938498")
ax.legend(loc='upper right', shadow=True)

# plot_nocollateral_allocated_ene.pdf
fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_title("Example 5")
ax.set_xlabel("Time / Years")
ax.set_ylabel("Exposure")
trade1_time = oreex.get_output_data_from_column("collateral_none/exposure_trade_70309.csv", 2)
trade1_epe = oreex.get_output_data_from_column("collateral_none/exposure_trade_70309.csv", 6)
trade2_time = oreex.get_output_data_from_column("collateral_none/exposure_trade_919020.csv", 2)
trade2_epe = oreex.get_output_data_from_column("collateral_none/exposure_trade_919020.csv", 6)
trade3_time = oreex.get_output_data_from_column("collateral_none/exposure_trade_938498.csv", 2)
trade3_epe = oreex.get_output_data_from_column("collateral_none/exposure_trade_938498.csv", 6)
ax.plot(trade1_time, trade1_epe, color='b', label="Allocated ENE 70309")
ax.plot(trade2_time, trade2_epe, color='r', label="Allocated ENE 919020")
ax.plot(trade3_time, trade3_epe, color='g', label="Allocated ENE 938498")
ax.legend(loc='upper right', shadow=True)

# plot_threshold_epe.pdf
fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_title("Example 5")
ax.set_xlabel("Time / Years")
ax.set_ylabel("Exposure")
ns_none_time = oreex.get_output_data_from_column(os.path.join("collateral_none","exposure_nettingset_CUST_A.csv"), 2)
ns_none_epe = oreex.get_output_data_from_column(os.path.join("collateral_none","exposure_nettingset_CUST_A.csv"), 3)
ns_thresh_time = oreex.get_output_data_from_column(os.path.join("collateral_threshold","exposure_nettingset_CUST_A.csv"), 2)
ns_thresh_epe = oreex.get_output_data_from_column(os.path.join("collateral_threshold","exposure_nettingset_CUST_A.csv"), 3)
ax.plot(ns_none_time, ns_none_epe, color='b', label="EPE NettingSet")
ax.plot(ns_thresh_time, ns_thresh_epe, color='r', label="EPE NettingSet, Threshold 1m")
ax.legend(loc='upper right', shadow=True)

# plot_threshold_allocated_epe.pdf
fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_title("Example 5")
ax.set_xlabel("Time / Years")
ax.set_ylabel("Exposure")
trade1_time = oreex.get_output_data_from_column(os.path.join("collateral_threshold","exposure_trade_70309.csv"), 2)
trade1_epe = oreex.get_output_data_from_column(os.path.join("collateral_threshold","exposure_trade_70309.csv"), 5)
trade2_time = oreex.get_output_data_from_column(os.path.join("collateral_threshold","exposure_trade_919020.csv"), 2)
trade2_epe = oreex.get_output_data_from_column(os.path.join("collateral_threshold","exposure_trade_919020.csv"), 5)
trade3_time = oreex.get_output_data_from_column(os.path.join("collateral_threshold","exposure_trade_938498.csv"), 2)
trade3_epe = oreex.get_output_data_from_column(os.path.join("collateral_threshold","exposure_trade_938498.csv"), 5)
ax.plot(trade1_time, trade1_epe, color='b', label="Allocated EPE 70309")
ax.plot(trade2_time, trade2_epe, color='r', label="Allocated EPE 919020")
ax.plot(trade3_time, trade3_epe, color='g', label="Allocated EPE 938498")
ax.legend(loc='upper right', shadow=True)

# plot_mta_epe.pdf
fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_title("Example 5")
ax.set_xlabel("Time / Years")
ax.set_ylabel("Exposure")
ns_none_time = oreex.get_output_data_from_column(os.path.join("collateral_none","exposure_nettingset_CUST_A.csv"), 2)
ns_none_epe = oreex.get_output_data_from_column(os.path.join("collateral_none","exposure_nettingset_CUST_A.csv"), 3)
ns_thresh_time = oreex.get_output_data_from_column(os.path.join("collateral_mta","exposure_nettingset_CUST_A.csv"), 2)
ns_thresh_epe = oreex.get_output_data_from_column(os.path.join("collateral_mta","exposure_nettingset_CUST_A.csv"), 3)
ax.plot(ns_none_time, ns_none_epe, color='b', label="EPE NettingSet")
ax.plot(ns_thresh_time, ns_thresh_epe, color='r', label="EPE NettingSet, MTA 100k")
ax.legend(loc='upper right', shadow=True)

# plot_mpor_epe.pdf
fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_title("Example 5")
ax.set_xlabel("Time / Years")
ax.set_ylabel("Exposure")
ns_none_time = oreex.get_output_data_from_column(os.path.join("collateral_none","exposure_nettingset_CUST_A.csv"), 2)
ns_none_epe = oreex.get_output_data_from_column(os.path.join("collateral_none","exposure_nettingset_CUST_A.csv"), 3)
ns_thresh_time = oreex.get_output_data_from_column(os.path.join("collateral_mpor","exposure_nettingset_CUST_A.csv"), 2)
ns_thresh_epe = oreex.get_output_data_from_column(os.path.join("collateral_mpor","exposure_nettingset_CUST_A.csv"), 3)
ax.plot(ns_none_time, ns_none_epe, color='b', label="EPE NettingSet")
ax.plot(ns_thresh_time, ns_thresh_epe, color='r', label="EPE NettingSet, MPOR 2W")
ax.legend(loc='upper right', shadow=True)

# plot_collateral.pdf
fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_title("Example 5")
ax.set_xlabel("Time / Years")
ax.set_ylabel("Collateral Balance")
col_time = oreex.get_output_data_from_column(os.path.join("collateral_threshold","colva_nettingset_CUST_A.csv"), 2)[1:]
col_bal = oreex.get_output_data_from_column(os.path.join("collateral_threshold","colva_nettingset_CUST_A.csv"), 3)[1:]
ax.plot(col_time, col_bal, color='b', label="Collateral Balance")
ax.legend(loc='upper right', shadow=True)

# plot_colva.pdf
fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_title("Example 5")
ax.set_xlabel("Time / Years")
ax.set_ylabel("COLVA")
col_time = oreex.get_output_data_from_column(os.path.join("collateral_threshold","colva_nettingset_CUST_A.csv"), 2)[1:]
col_bal = oreex.get_output_data_from_column(os.path.join("collateral_threshold","colva_nettingset_CUST_A.csv"), 4)[1:]
ax.plot(col_time, col_bal, color='b', label="COLVA Increments")
ax.legend(loc='upper right', shadow=True)

# plot_collateral_floor.pdf
fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_title("Example 5")
ax.set_xlabel("Time / Years")
ax.set_ylabel("Collateral Floor Value")
col_time = oreex.get_output_data_from_column(os.path.join("collateral_threshold","colva_nettingset_CUST_A.csv"), 2, 2)
col_bal = oreex.get_output_data_from_column(os.path.join("collateral_threshold","colva_nettingset_CUST_A.csv"), 6, 2)
ax.plot(col_time, col_bal, color='b', label="Collateral Floor Increments")
ax.legend(loc='upper right', shadow=True)

# plot_threshold_break_epe.pdf
fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_title("Example 5")
ax.set_xlabel("Time / Years")
ax.set_ylabel("Exposure")
col_thresh_time = oreex.get_output_data_from_column(os.path.join("collateral_threshold","exposure_nettingset_CUST_A.csv"), 2)
col_thresh_epe = oreex.get_output_data_from_column(os.path.join("collateral_threshold","exposure_nettingset_CUST_A.csv"), 3)
col_thresh_break_time = oreex.get_output_data_from_column(os.path.join("collateral_threshold_break","exposure_nettingset_CUST_A.csv"), 2)
col_thresh_break_epe = oreex.get_output_data_from_column(os.path.join("collateral_threshold_break","exposure_nettingset_CUST_A.csv"), 3)
ax.plot(col_thresh_time, col_thresh_epe, color='b', label="EPE NettingSet, Threshold 1m")
ax.plot(col_thresh_break_time, col_thresh_break_epe, color='r', label="EPE NettingSet, Threshold 1m, Breaks")
ax.legend(loc='upper right', shadow=True)

plt.show()

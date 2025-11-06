#!/usr/bin/env python

import sys
import os
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE classic")
oreex.run("Input/ore_classic.xml")
#oreex.get_times("Output/classic/log.txt")

oreex.print_headline("Run ORE AMC")
oreex.run("Input/ore_amc.xml")
#oreex.get_times("Output/amc/log.txt")

oreex.setup_plot("exposure")
oreex.plot(os.path.join("classic", "exposure_nettingset_CPTY_A.csv"), 9, 10, 'b', "Base EPE Classic", filter='Base')
oreex.plot(os.path.join("classic", "exposure_nettingset_CPTY_A.csv"), 9, 11, 'r', "Base ENE Classic", filter='Base')
oreex.plot(os.path.join("amc", "exposure_nettingset_CPTY_A.csv"), 9, 10, 'c', "Base EPE AMC", filter='Base')
oreex.plot(os.path.join("amc", "exposure_nettingset_CPTY_A.csv"), 9, 11, 'y', "Base ENE AMC", filter='Base')
oreex.decorate_plot(title="Example 68, Exposure Base Scenario", xlabel="Timestep", ylabel="EPE", legend_loc="upper right")
oreex.save_plot_to_file()

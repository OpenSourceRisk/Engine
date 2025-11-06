#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| XVA Risk: XVA Sensitivity Analysis                  |")
print("+-----------------------------------------------------+")

# Legacy example 68

oreex.print_headline("Run XVA Sensitivity classic")
oreex.run("Input/ore_sensi_classic.xml")

oreex.print_headline("Run XVA Sensitivity AMC")
oreex.run("Input/ore_sensi_amc.xml")

oreex.setup_plot("exposure")
oreex.plot("sensi/classic/exposure_nettingset_CPTY_A.csv", 9, 10, 'b', "Base EPE Classic", filter='Base')
oreex.plot("sensi/classic/exposure_nettingset_CPTY_A.csv", 9, 11, 'r', "Base ENE Classic", filter='Base')
oreex.plot("sensi/amc/exposure_nettingset_CPTY_A.csv", 9, 10, 'c', "Base EPE AMC", filter='Base')
oreex.plot("sensi/amc/exposure_nettingset_CPTY_A.csv", 9, 11, 'y', "Base ENE AMC", filter='Base')
oreex.decorate_plot(title="Exposure Base Scenario", xlabel="Timestep", ylabel="EPE", legend_loc="upper right")
oreex.save_plot_to_file()

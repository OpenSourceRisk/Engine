#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

# oreex.print_headline("Run ORE to produce NPV cube and exposures")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Run ORE again to price CDS Options")
oreex.run("Input/ore_cds_option.xml")

# oreex.print_headline("Plot results: Simulated exposures vs analytical cds option prices")

oreex.setup_plot("cds options")
oreex.plot("exposure_trade_CDS.csv", 2, 3, 'b', "CDS EPE")
oreex.plot("exposure_trade_CDS.csv", 2, 4, 'r', "CDS ENE")
oreex.plot("cds_option_npv.csv", 3, 4, 'g', "NPV CDS Ooptions", marker='s')
oreex.decorate_plot(title="Example 35 - Simulated exposures vs analytical cds option prices")
oreex.save_plot_to_file()


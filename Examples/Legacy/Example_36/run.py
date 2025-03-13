#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

# LGM
oreex.print_headline("Run ORE simulation in the LGM measure")
oreex.run("Input/ore_lgm.xml")
oreex.save_output_to_subdir(
    "measure_lgm",
    ["log.txt", "xva.csv"] + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
)

# FWD
oreex.print_headline("Run ORE simulation in the FWD measure")
oreex.run("Input/ore_fwd.xml")
oreex.save_output_to_subdir(
    "measure_fwd",
    ["log.txt", "xva.csv"] + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
)

# BA
oreex.print_headline("Run ORE simulation in the BA measure")
oreex.run("Input/ore_ba.xml")
oreex.save_output_to_subdir(
    "measure_ba",
    ["log.txt", "xva.csv"] + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
)

oreex.print_headline("Plot results")

oreex.setup_plot("exposures_measures")
oreex.plot(os.path.join("measure_lgm", "exposure_nettingset_CPTY_A.csv"), 2, 5, 'b', "PFE LGM")
oreex.plot(os.path.join("measure_ba", "exposure_nettingset_CPTY_A.csv"), 2, 5, 'g', "PFE BA")
oreex.plot(os.path.join("measure_fwd", "exposure_nettingset_CPTY_A.csv"), 2, 5, 'r', "PFE FWD")
oreex.plot(os.path.join("measure_lgm", "exposure_nettingset_CPTY_A.csv"), 2, 3, 'b', "EPE LGM")
oreex.plot(os.path.join("measure_ba", "exposure_nettingset_CPTY_A.csv"), 2, 3, 'g', "EPE BA")
oreex.plot(os.path.join("measure_fwd", "exposure_nettingset_CPTY_A.csv"), 2, 3, 'r', "EPE FWD")
oreex.decorate_plot(title="Example 36")
oreex.save_plot_to_file()

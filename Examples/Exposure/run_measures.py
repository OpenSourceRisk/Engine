#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+------------------------------------------------+")
print("| Exposure: Choice of Measure, PFE Impact        |")
print("+------------------------------------------------+")

# Legacy Example 36

oreex.print_headline("Run ORE simulation in the LGM measure")
oreex.run("Input/ore_measure_lgm.xml")

oreex.print_headline("Run ORE simulation in the FWD measure")
oreex.run("Input/ore_measure_fwd.xml")

oreex.print_headline("Run ORE simulation in the BA measure")
oreex.run("Input/ore_measure_ba.xml")

oreex.print_headline("Plot results")

oreex.setup_plot("exposures_measures")
oreex.plot("measure_lgm/exposure_nettingset_CPTY_A.csv", 2, 5, 'b', "PFE LGM")
oreex.plot("measure_ba/exposure_nettingset_CPTY_A.csv", 2, 5, 'g', "PFE BA")
oreex.plot("measure_fwd/exposure_nettingset_CPTY_A.csv", 2, 5, 'r', "PFE FWD")
oreex.plot("measure_lgm/exposure_nettingset_CPTY_A.csv", 2, 3, 'b', "EPE LGM")
oreex.plot("measure_ba/exposure_nettingset_CPTY_A.csv", 2, 3, 'g', "EPE BA")
oreex.plot("measure_fwd/exposure_nettingset_CPTY_A.csv", 2, 3, 'r', "EPE FWD")
oreex.decorate_plot(title="Choice of Measure")
oreex.save_plot_to_file()

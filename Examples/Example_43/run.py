#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot results: Loss distribution")

oreex.setup_plot("Loss Distribution")
oreex.plot("credit_migration_11.csv", 0, 2, 'b', label="Loss")
oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="CDF", xlabel="Loss", y_format_as_int=False)
oreex.save_plot_to_file()


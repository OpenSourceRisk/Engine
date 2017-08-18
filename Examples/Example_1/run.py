#!/usr/bin/env python

import sys
import imp
ore_examples_helper = imp.load_source('ore_examples_helper', '../ore_examples_helper.py')


def run_example():

    oreex = ore_examples_helper.OreExample(sys.argv[1] if len(sys.argv)>1 else False)

    oreex.print_headline("Run ORE to produce NPV cube and exposures")
    oreex.run("Input/ore.xml")
    oreex.get_times("Output/log.txt")

    oreex.print_headline("Run ORE again to price European Swaptions")
    oreex.run("Input/ore_swaption.xml")

    oreex.print_headline("Plot results: Simulated exposures vs analytical swaption prices")

    oreex.setup_plot("swaptions")
    oreex.plot("exposure_trade_Swap_20y.csv", 2, 3, 'b', "Swap EPE")
    oreex.plot("exposure_trade_Swap_20y.csv", 2, 4, 'r', "Swap ENE")
    oreex.plot("swaption_npv.csv", 3, 4, 'g', "NPV Swaptions", marker='s')
    oreex.decorate_plot(title="Example 1 - Simulated exposures vs analytical swaption prices")
    oreex.save_plot_to_file()

    return oreex

if __name__ == "__main__":
    run_example()

#!/usr/bin/env python

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

import sys
oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

## Portfolio 1 run
#oreex.print_headline("Run ORE to produce NPV cube and exposures for portfolio 1")
#oreex.run("Input/ore_portfolio_1.xml")
#oreex.get_times("Output/portfolio_1/log.txt")
# 
#oreex.print_headline("Plot results for portfolio 1")
# 
#oreex.setup_plot("portfolio_1")
#oreex.plot(os.path.join("portfolio_1", "exposure_trade_swap_01.csv"), 2, 3, 'b', "EPE Swap")
#oreex.plot(os.path.join("portfolio_1", "exposure_trade_collar_01.csv"), 2, 4, 'r', "ENE Collar")
#oreex.plot(os.path.join("portfolio_1", "exposure_nettingset_CPTY_A.csv"), 2, 3, 'g', "EPE Netting")
#oreex.decorate_plot(title="Example 12, Portfolio 1")
#oreex.save_plot_to_file(os.path.join("Output", "portfolio_1"))
#
## Portfolio 2 run
#oreex.print_headline("Run ORE to produce NPV cube and exposures for portfolio 2")
#oreex.run("Input/ore_portfolio_2.xml")
#oreex.get_times("Output/portfolio_2/log.txt")
#
#oreex.print_headline("Plot results for portfolio 2")
#
#oreex.setup_plot("portfolio_2")
#oreex.plot(os.path.join("portfolio_2", "exposure_trade_floor_01.csv"), 2, 3, 'b', "EPE Floor")
#oreex.plot(os.path.join("portfolio_2", "exposure_trade_cap_01.csv"), 2, 4, 'r', "ENE Cap")
#oreex.plot(os.path.join("portfolio_2", "exposure_nettingset_CPTY_B.csv"), 2, 3, 'g', "EPE Net Cap and Floor")
#oreex.plot(os.path.join("portfolio_2", "exposure_trade_collar_02.csv"), 2, 4, 'g', "ENE Collar", offset=1, marker='o', linestyle='')
#oreex.decorate_plot(title="Example 12, Portfolio 2")
#oreex.save_plot_to_file(os.path.join("Output", "portfolio_2"))
#
## Portfolio 3 run
#oreex.print_headline("Run ORE to produce NPV cube and exposures for portfolio 3")
#oreex.run("Input/ore_portfolio_3.xml")
#oreex.get_times("Output/portfolio_3/log.txt")
#
#oreex.print_headline("Plot results for portfolio 3")
#
#oreex.setup_plot("portfolio_3")
#oreex.plot(os.path.join("portfolio_3", "exposure_trade_cap_02.csv"), 2, 3, 'b', "EPE Cap")
#oreex.plot(os.path.join("portfolio_3", "exposure_trade_cap_03.csv"), 2, 4, 'r', "ENE Amortising Cap")
#oreex.plot(os.path.join("portfolio_3", "exposure_nettingset_CPTY_B.csv"), 2, 3, 'g', "EPE Netted")
#oreex.decorate_plot(title="Example 12, Portfolio 3")
#oreex.save_plot_to_file(os.path.join("Output", "portfolio_3"))

# Portfolio 4 run
oreex.print_headline("Run ORE to produce NPV cube and exposures for portfolio 4")
#oreex.run("Input/ore_portfolio_4.xml")
oreex.get_times("Output/portfolio_4/log.txt")

oreex.print_headline("Plot results for portfolio 4")

oreex.setup_plot("portfolio_4")
oreex.plot(os.path.join("portfolio_4", "exposure_nettingset_CPTY_A.csv"), 2, 3, 'b', "EPE Swap + Collar")
oreex.plot(os.path.join("portfolio_4", "exposure_nettingset_CPTY_A.csv"), 2, 4, 'r', "ENE Swap + Collar")
oreex.plot(os.path.join("portfolio_4", "exposure_nettingset_CPTY_B.csv"), 2, 3, 'b', "EPE CapFloored Swap", offset=1, marker='o', linestyle='')
oreex.plot(os.path.join("portfolio_4", "exposure_nettingset_CPTY_B.csv"), 2, 4, 'r', "ENE CapFloored Swap", offset=1, marker='o', linestyle='')
oreex.decorate_plot(title="Example 12, Portfolio 4")
oreex.save_plot_to_file(os.path.join("Output", "portfolio_4"))

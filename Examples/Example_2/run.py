#!/usr/bin/env python

import os
import runpy
ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

oreex = OreExample()

oreex.print_headline("Run ORE to produce NPV cube and exposures")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

npv = open("Output/npv.csv")
call = []
put = []
for line in npv.readlines():
    if "CALL" in line:
        line_list = line.split(',')
        call=([[0,line_list[6]],[line_list[3], line_list[6]]])
    if "PUT" in line:
        line_list = line.split(',')
        put=([[0, line_list[6]],[line_list[3],line_list[6]]])

oreex.setup_plot("forward")
oreex.plot("exposure_trade_FXFWD_EURUSD_10Y.csv", 2, 3, 'b', "EPE")
oreex.plot("exposure_trade_FXFWD_EURUSD_10Y.csv", 2, 4, 'r', "ENE")
oreex.plot_line([0, call[1][0]], [call[0][1], call[1][1]], color='g', label="Call Price")
oreex.plot_line([0, put[1][0]], [put[0][1], put[1][1]], color='m', label="Put Price")
oreex.decorate_plot(title="Example 2 - FX Forward")
oreex.save_plot_to_file()

oreex.setup_plot("option")
oreex.plot("exposure_trade_FX_CALL_OPTION_EURUSD_10Y.csv", 2, 3, 'b', "EPE")
oreex.plot("exposure_trade_FX_PUT_OPTION_EURUSD_10Y.csv", 2, 3, 'r', "ENE")
oreex.plot_line([0, call[1][0]], [call[0][1], call[1][1]], color='g', label="Call Price")
oreex.plot_line([0, put[1][0]], [put[0][1], put[1][1]], color='m', label="Put Price")
oreex.decorate_plot(title="Example 2 - FX Option")
oreex.save_plot_to_file()

#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposures: FX Forward/Option                     |")
print("+--------------------------------------------------+")

# Legacy example 7

oreex.run("Input/ore_fx.xml")

npv = open("Output/fx/npv.csv")
call = []
put = []
for line in npv.readlines():
    if "CALL" in line:
        line_list = line.split(',')
        call=([[0.0,float(line_list[6])],[float(line_list[3]), float(line_list[6])]])
    if "PUT" in line:
        line_list = line.split(',')
        put=([[0.0, float(line_list[6])],[float(line_list[3]), float(line_list[6])]])

oreex.setup_plot("exposure_fxforward")
oreex.plot("fx/exposure_trade_FXFWD_EURUSD_10Y.csv", 2, 3, 'b', "EPE")
oreex.plot("fx/exposure_trade_FXFWD_EURUSD_10Y.csv", 2, 4, 'r', "ENE")
oreex.plot_line([0, call[1][0]], [call[0][1], call[1][1]], color='g', label="Call Price")
oreex.plot_line([0, put[1][0]], [put[0][1], put[1][1]], color='m', label="Put Price")
oreex.decorate_plot(title="FX Forward")
oreex.save_plot_to_file()

oreex.setup_plot("exposure_fxoption")
oreex.plot("fx/exposure_trade_FX_CALL_OPTION_EURUSD_10Y.csv", 2, 3, 'b', "EPE")
oreex.plot("fx/exposure_trade_FX_PUT_OPTION_EURUSD_10Y.csv", 2, 3, 'r', "ENE")
oreex.plot_line([0, call[1][0]], [call[0][1], call[1][1]], color='g', label="Call Price")
oreex.plot_line([0, put[1][0]], [put[0][1], put[1][1]], color='m', label="Put Price")
oreex.decorate_plot(title="FX Option")
oreex.save_plot_to_file()



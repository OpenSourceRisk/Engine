#!/usr/bin/env python

import os
import matplotlib.pyplot as plt
import runpy

ore_helpers = runpy.run_path(os.path.join(os.path.dirname(os.getcwd()), "ore_examples_helper.py"))
OreExample = ore_helpers['OreExample']

oreex = OreExample()

oreex.print_headline("Run ORE to produce NPV cube and exposures")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

# get call and put data
npv = open("Output/npv.csv")
call = []
put = []
for line in npv.readlines():
    if "CALL" in line:
        line_list = line.split(',')
        call=([[str(0),str(line_list[6])],[str(line_list[3]),str(line_list[6])]])
    if "PUT" in line:
        line_list = line.split(',')
        put=([[str(0),str(line_list[6])],[str(line_list[3]),str(line_list[6])]])
with open("Output/call.csv", 'w') as f: # not neccessary for plotting
    for entry in call:
        f.writelines(','.join(entry)+"\n")
with open("Output/put.csv", 'w') as f:
    for entry in put:
        f.writelines(','.join(entry)+"\n")

# plot forward
fig = plt.figure()
ax = fig.add_subplot(111)
oreex.plot(ax, "exposure_trade_FXFWD_EURUSD_10Y.csv", 2, 3, 'b', "EPE")
oreex.plot(ax, "exposure_trade_FXFWD_EURUSD_10Y.csv", 2, 4, 'r', "ENE")
ax.plot([0, float(call[1][0])], [float(call[0][1]), float(call[1][1])], color='g', label="Call Price")
ax.plot([0, float(put[1][0])], [float(put[0][1]), float(put[1][1])], color='m', label="Put Price")
oreex.decorate_plot(ax, title="Example 2 - FX Forward")


fig = plt.figure()
ax = fig.add_subplot(111)
oreex.plot(ax, "exposure_trade_FX_CALL_OPTION_EURUSD_10Y.csv", 2, 3, 'b', "EPE")
oreex.plot(ax, "exposure_trade_FX_PUT_OPTION_EURUSD_10Y.csv", 2, 3, 'r', "ENE")
ax.plot([0, float(call[1][0])], [float(call[0][1]), float(call[1][1])], color='g', label="Call Price")
ax.plot([0, float(put[1][0])], [float(put[0][1]), float(put[1][1])], color='m', label="Put Price")
ax.legend(loc='upper right', shadow=True)
oreex.decorate_plot(ax, title="Example 2 - FX Option")

plt.show()

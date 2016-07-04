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
fig1 = plt.figure()
ax1 = fig1.add_subplot(111)
ax1.set_title("Example 2 - FX Forward")
ax1.set_xlabel("Time / Years")
ax1.set_ylabel("Exposure")
time_fw = oreex.get_output_data_from_column("exposure_trade_FXFWD_EURUSD_10Y.csv", 2)
epe_fw = oreex.get_output_data_from_column("exposure_trade_FXFWD_EURUSD_10Y.csv", 3)
ene_fw = oreex.get_output_data_from_column("exposure_trade_FXFWD_EURUSD_10Y.csv", 4)
ax1.plot(time_fw, epe_fw, color='b', label="EPE")
ax1.plot(time_fw, ene_fw, color='r', label="ENE")
ax1.plot([0, float(call[1][0])], [float(call[0][1]), float(call[1][1])], color='g', label="Call Price")
ax1.plot([0, float(put[1][0])], [float(put[0][1]), float(put[1][1])], color='m', label="Put Price")
ax1.legend(loc='upper right', shadow=True)

fig2 = plt.figure()
ax2 = fig2.add_subplot(111)
ax2.set_title("Example 2 - FX Option")
ax2.set_xlabel("Time / Years")
ax2.set_ylabel("Exposure")
call_time = oreex.get_output_data_from_column("exposure_trade_FX_CALL_OPTION_EURUSD_10Y.csv", 2)
call_epe = oreex.get_output_data_from_column("exposure_trade_FX_CALL_OPTION_EURUSD_10Y.csv", 3)
put_time = oreex.get_output_data_from_column("exposure_trade_FX_PUT_OPTION_EURUSD_10Y.csv", 2)
put_epe = oreex.get_output_data_from_column("exposure_trade_FX_PUT_OPTION_EURUSD_10Y.csv", 3)
ax2.plot(call_time, call_epe, color='b', label="EPE")
ax2.plot(put_time, put_epe, color='r', label="ENE")
ax2.plot([0, float(call[1][0])], [float(call[0][1]), float(call[1][1])], color='g', label="Call Price")
ax2.plot([0, float(put[1][0])], [float(put[0][1]), float(put[1][1])], color='m', label="Put Price")
ax2.legend(loc='upper right', shadow=True)

plt.show()

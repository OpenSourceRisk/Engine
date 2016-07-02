#!/usr/bin/env python

import os
import sys
import subprocess
import matplotlib.pyplot as plt

# locate ore.exe
ore_exe = os.getcwd()
ore_exe = os.path.dirname(ore_exe) # one level up
ore_exe = os.path.dirname(ore_exe) # one level up
ore_exe = os.path.join(ore_exe, "App")
ore_exe = os.path.join(ore_exe, "bin")
ore_exe = os.path.join(ore_exe, "Win32" if sys.platform == "win32" else "x64")
ore_exe = os.path.join(ore_exe, "Release")
ore_exe = os.path.join(ore_exe, "ore.exe")


# run simulation and exposure postprocessor
print("1) Run ORE to produce NPV cube and exposures")
subprocess.call([ore_exe, "Input/ore.xml"])

# check times
print("Get times from the log file:")
logfile = open("Output/log.txt")
for line in logfile.readlines():
    if "ValuationEngine completed" in line:
        times = line.split(":")[-1].strip().split(",")
        for time in times:
            print("\t" + time.split()[0] + ": "  + time.split()[1])
print()

# run swaption pricing
print("2) Run ORE again to price European Swaptions")
subprocess.call([ore_exe, "Input/ore_swaption.xml"])
print()

# plot
print("3) Plot results: Simulated exposures vs analytical swaption prices")
# get exposure swap data
exposure_swap = open(os.path.join(os.path.join(os.getcwd(), "Output"), "exposure_trade_Swap_20y.csv"))
time_swap = []
epes = []
enes = []
for line in exposure_swap:
    time_swap.append(line.split(',')[2])
    epes.append(line.split(',')[3])
    enes.append(line.split(',')[4])
time_swap = time_swap[1:]
epes = epes[1:]
enes = enes[1:]

# get swaptions data
swaptions = open(os.path.join(os.path.join(os.getcwd(), "Output"), "swaption_npv.csv"))
times_swaption = []
npv = []
for line in swaptions:
    npv.append(line.split(',')[4])
    times_swaption.append(line.split(',')[3])
npv = npv[1:]
times_swaption = times_swaption[1:]

# present plot
fig = plt.figure()
ax = fig.add_subplot(111)
ax.plot(time_swap, epes, color='b', label="EPE")
ax.plot(time_swap, enes, color='r', label="ENE")
ax.plot(times_swaption, npv, color='g', label="NPV")
ax.legend(loc='upper right', shadow=True)
ax.set_xlabel("Time / Years")
ax.set_ylabel("Exposure")
ax.set_title("Example 1")
plt.show()

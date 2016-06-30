#!/usr/bin/env python

import os
import sys
import subprocess

def get_platform_string():
    return "Win32" if sys.platform == "win32" else "x64"

# locate ore.exe
ore_exe = os.getcwd()
ore_exe = os.path.dirname(ore_exe) # one level up
ore_exe = os.path.dirname(ore_exe) # one level up
ore_exe = os.path.join(ore_exe, "App")
ore_exe = os.path.join(ore_exe, "bin")
ore_exe = os.path.join(ore_exe, get_platform_string())
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
subprocess.call([ore_exe, "Input/ore.xml"])
print()

# plot
print("3) Plot results: Simulated exposures vs analytical swaption prices")
plotfile = os.path.join(os.path.join(os.getcwd(),"Output"),"plot.gp")
print(plotfile)
subprocess.call(["gnuplot", plotfile])
print()

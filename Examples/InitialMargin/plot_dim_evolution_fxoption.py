#!/usr/bin/env python

import os
import sys
import pandas as pd
import lxml.etree as etree
import utilities
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Plot Dynamic SIMM                                   |")
print("+-----------------------------------------------------+")

oreex.setup_plot("simm_evolution_expected")
oreex.plot("Dim2/AmcCg/dim_evolution.csv", 8, 4, 'r', "Dynamic SIMM")
oreex.plot("Dim2/SimmAnalytic/dim_evolution.csv", 6, 3, 'b', "Benchmark II")
oreex.plot("Dim2/AmcReg/dim_evolution.csv", 8, 4, 'g', "Regression Scaled")
oreex.plot("Dim2/AmcRegUnscaled/dim_evolution.csv", 8, 4, 'y', "Regression Unscaled")
oreex.decorate_plot(title="Expected SIMM Evolution", ylabel="SIMM", xlabel="Time",legend_loc="lower left")
oreex.save_plot_to_file()

amcCubeFile = "Output/Dim2/AmcCg/dim_cube.csv"
asCubeFile = "Output/Dim2/SimmAnalytic/dim_cube.csv"
simmCubeFile = "Output/DimValidation/simm_cube.csv"
regCubeFile = "Output/Dim2/AmcReg/dim_cube.csv"
reguCubeFile = "Output/Dim2/AmcRegUnscaled/dim_cube.csv"

def pathwiseComparison(s, oreex, color1, color2, color3, color4, color5, depth):
    amcEvolutionFile = "Dim2/AmcCg/dim_evolution_" + str(s) + "_" + str(depth) + ".csv"
    utilities.simmEvolution(s - 1, amcCubeFile, "Output/" + amcEvolutionFile, depth)
    print("evolution file", amcEvolutionFile, "written")

    asEvolutionFile = "Dim2/SimmAnalytic/dim_evolution_" + str(s) + "_" + str(depth) + ".csv"
    utilities.simmEvolution(s - 1, asCubeFile, "Output/" + asEvolutionFile, depth)
    print("evolution file", asEvolutionFile, "written")

    simmEvolutionFile = "DimValidation/simm_evolution_" + str(s) + "_" + str(depth) + ".csv"
    utilities.simmEvolution(s, simmCubeFile, "Output/" + simmEvolutionFile, depth)
    print("evolution file", simmEvolutionFile, "written")

    regEvolutionFile = "Dim2/AmcReg/dim_evolution_" + str(s) + "_" + str(depth) + ".csv"
    utilities.simmEvolution(s - 1, regCubeFile, "Output/" + regEvolutionFile, depth)
    print("evolution file", regEvolutionFile, "written")

    reguEvolutionFile = "Dim2/AmcRegUnscaled/dim_evolution_" + str(s) + "_" + str(depth) + ".csv"
    utilities.simmEvolution(s - 1, reguCubeFile, "Output/" + reguEvolutionFile, depth)
    print("evolution file", reguEvolutionFile, "written")

    oreex.plot(simmEvolutionFile, 1, 5, color2, "Benchmark I")
    oreex.plot(asEvolutionFile, 1, 5, color3, "Benchmark II")
    oreex.plot(amcEvolutionFile, 1, 5, color1, "AMC/AAD")

sampleRange = [1, 2, 3, 4, 5, 6, 7, 8]
#depthRange = [0, 1, 2, 3]
depthRange = [0]

# one by one benchmarking

for s in sampleRange:
    for d in depthRange:
        print("sample", s, "depth", d)
        oreex.setup_plot("simm_evolution_pathwise_" + str(s) + "_" + str(d), 0.75)
        pathwiseComparison(s, oreex, 'r', 'b', 'black', 'g', 'y', d)    
        if d == 0:
            oreex.decorate_plot(title="SIMM Evolution Pathwise, Sample " + str(s),
                                ylabel="SIMM", xlabel="Time", legend_loc="lower left")
        else:
            oreex.decorate_plot(title="SIMM Evolution Pathwise, Depth " + str(d) + ", Sample " + str(s),
                                ylabel="SIMM", xlabel="Time", legend_loc="lower left")
        oreex.save_plot_to_file()

print("done")

# Dynamic SIMM paths in one plot, depth 0 only

sampleRange = [2, 5, 6, 7]
colorRange = ['r', 'b', 'g', 'c']
depthRange = [0]

oreex.setup_plot("simm_evolution_pathwise")
for i in range (0,len(sampleRange)):
    s = sampleRange[i]
    c = colorRange[i]
    print("sample", s)

    amcEvolutionFile = "Dim2/AmcCg/dim_evolution_" + str(s) + ".csv"
    utilities.simmEvolution(s - 1, amcCubeFile, "Output/" + amcEvolutionFile, 0)
    print("evolution file", amcEvolutionFile, "written")

    oreex.plot(amcEvolutionFile, 1, 5, c, "Sample " + str(s))

oreex.decorate_plot(title="SIMM Evolution Pathwise",
                    ylabel="SIMM", xlabel="Time", legend_loc="lower left")
oreex.save_plot_to_file()

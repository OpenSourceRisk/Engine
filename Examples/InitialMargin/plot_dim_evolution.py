#!/usr/bin/env python

import glob
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


#oreex.setup_plot("simm_evolution_expected")
#oreex.plot("DimValidation/simm_evolution_avg.csv", 1, 5, 'black', "SIMM Average")
#oreex.plot("Dim2/AmcCg/dim_evolution.csv", 8, 4, 'r', "Expected Dynamic SIMM")
#oreex.plot("Dim2/AmcReg/dim_evolution.csv", 8, 4, 'b', "Expected Regression DIM")
#oreex.decorate_plot(title="SIMM Evolution", ylabel="SIMM", xlabel="Time",legend_loc="lower left")
#oreex.save_plot_to_file()

amcCubeFile = "Output/Dim2/AmcCg/dim_cube.csv"
asCubeFile = "Output/Dim2/SimmAnalytic/dim_cube.csv"
simmCubeFile = "Output/DimValidation/simm_cube.csv"

def pathwiseComparison(s, oreex, color1, color2, color3, depth):
    amcEvolutionFile = "Dim2/AmcCg/dim_evolution_" + str(s) + "_" + str(depth) + ".csv"
    utilities.simmEvolution(s - 1, amcCubeFile, "Output/" + amcEvolutionFile, depth)
    print("evolution file", amcEvolutionFile, "written")

    asEvolutionFile = "Dim2/SimmAnalytic/dim_evolution_" + str(s) + "_" + str(depth) + ".csv"
    utilities.simmEvolution(s - 1, asCubeFile, "Output/" + asEvolutionFile, depth)
    print("evolution file", asEvolutionFile, "written")

    simmEvolutionFile = "DimValidation/simm_evolution_" + str(s) + "_" + str(depth) + ".csv"
    utilities.simmEvolution(s, simmCubeFile, "Output/" + simmEvolutionFile, depth)
    print("evolution file", simmEvolutionFile, "written")

    oreex.plot(simmEvolutionFile, 1, 5, color3, "SIMM Sample " + str(s))
    oreex.plot(amcEvolutionFile, 1, 5, color2, "DIM AMC Sample " + str(s))
    oreex.plot(asEvolutionFile, 1, 5, color1, "DIM Classic Sample " + str(s))


sampleRange = [1, 2, 3, 4, 5, 6, 7, 8]
depthRange = [0, 1, 2, 3]

for s in sampleRange:
    for d in depthRange:
        print("sample", s, "depth", d)
        oreex.setup_plot("simm_evolution_pathwise_" + str(s) + "_" + str(d))
        pathwiseComparison(s, oreex, 'r', 'b', 'black', d)    
        oreex.decorate_plot(title="SIMM Evolution Pathwise, Depth " + str(d),
                            ylabel="SIMM", xlabel="Time", legend_loc="upper right")
        oreex.save_plot_to_file()

print("done")

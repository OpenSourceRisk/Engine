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

dimCubeFile = "Output/Dim2/AmcCg/dim_cube.csv"
dimCubeFile2 = "Output/Dim2/SimmAnalytic/dim_cube.csv"
simmCubeFile = "Output/DimValidation/simm_cube.csv"

def pathwiseComparison(s, oreex, color1, color2, color3, depth):
    dimEvolutionFile = "Dim2/AmcCg/dim_evolution_" + str(s) + "_" + str(depth) + ".csv"
    utilities.simmEvolution(s - 1, dimCubeFile, "Output/" + dimEvolutionFile, depth)
    print("evolution file", dimEvolutionFile, "written")

    dimEvolutionFile2 = "Dim2/SimmAnalytic/dim_evolution_" + str(s) + "_" + str(depth) + ".csv"
    utilities.simmEvolution(s - 1, dimCubeFile2, "Output/" + dimEvolutionFile2, depth)
    print("evolution file", dimEvolutionFile, "written")

    simmEvolutionFile = "DimValidation/simm_evolution_" + str(s) + "_" + str(depth) + ".csv"
    utilities.simmEvolution(s, simmCubeFile, "Output/" + simmEvolutionFile, depth)
    print("evolution file", simmEvolutionFile, "written")

    oreex.plot(simmEvolutionFile, 1, 5, color3, "SIMM Sample " + str(s))
    oreex.plot(dimEvolutionFile, 1, 5, color2, "DIM AMC/AD Sample " + str(s))
    oreex.plot(dimEvolutionFile2, 1, 5, color1, "DIM Classic Sample " + str(s))

oreex.setup_plot("simm_evolution_pathwise_0")
#pathwiseComparison(2, oreex, 'r', 'g', 0)    
pathwiseComparison(8, oreex, 'r', 'b', 'black', 0)    
#pathwiseComparison(7, oreex, 'b', 'm', 0)    
#pathwiseComparison(4, oreex, 'r', 'g', 0)    
#pathwiseComparison(5, oreex, 'r', 'g', 0)    
#pathwiseComparison(6, oreex, 'b', 'm', 0)    
oreex.decorate_plot(title="SIMM Evolution Pathwise, Depth 0", ylabel="SIMM", xlabel="Time",legend_loc="upper right")
oreex.save_plot_to_file()

#oreex.setup_plot("simm_evolution_pathwise_1")
#pathwiseComparison(2, oreex, 'r', 'g', 1)    
#pathwiseComparison(7, oreex, 'b', 'm', 1)    
#oreex.decorate_plot(title="SIMM Evolution Pathwise, Depth 1", ylabel="SIMM", xlabel="Time",legend_loc="lower left")
#oreex.save_plot_to_file()

#oreex.setup_plot("simm_evolution_pathwise_2")
#pathwiseComparison(2, oreex, 'r', 'g', 2)
#pathwiseComparison(7, oreex, 'b', 'm', 2)    
#oreex.decorate_plot(title="SIMM Evolution Pathwise, Depth 2", ylabel="SIMM", xlabel="Time",legend_loc="lower left")
#oreex.save_plot_to_file()

#oreex.setup_plot("simm_evolution_pathwise_3")
#pathwiseComparison(2, oreex, 'r', 'g', 3)    
#pathwiseComparison(7, oreex, 'b', 'm', 3)    
#oreex.decorate_plot(title="SIMM Evolution Pathwise, Depth 3", ylabel="SIMM", xlabel="Time",legend_loc="lower left")
#oreex.save_plot_to_file()

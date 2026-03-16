#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| CVA Sensis using AMC, AAD and GPU      |")
print("+----------------------------------------+")

oreex.print_headline("Run CVA Sensi with bump & reval")
oreex.run("Input/ore_cvasensi_bump.xml")

oreex.print_headline("Run CVA Sensi with AAD")
oreex.run("Input/ore_cvasensi_ad.xml")

oreex.print_headline("Run with bump & reval using a GPU")
oreex.run("Input/ore_cvasensi_gpu.xml")

#oreex.print_headline("Run with CG Cube")
#oreex.run("Input/ore_cvasensi_cgcube.xml")

#oreex.print_headline("Run with CG Cube and GPU")
#oreex.run("Input/ore_cvasensi_cgcube_gpu.xml")


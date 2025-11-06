#!/usr/bin/env python

import glob
import os
import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce normal xva from BANKs view")
oreex.run("Input/ore_Normal.xml")
oreex.get_times("Output/log.txt")
oreex.save_output_to_subdir(
    "NormalXVA",
    ["log.txt", "xva.csv"]
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
)

oreex.print_headline("Run ORE to produce flipped xva from CPTY_A's view")
oreex.run("Input/ore_FlipView.xml")
oreex.get_times("Output/log.txt")
oreex.save_output_to_subdir(
    "FlippedXVA",
    ["log.txt", "xva.csv"]
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
)

oreex.print_headline("Run ORE to produce normal xva with reversed swap (identical to flipped xva as BANK and CPTY_A have identical Default and fva curves)")
oreex.run("Input/ore_ReversedNormal.xml")
oreex.get_times("Output/log.txt")
oreex.save_output_to_subdir(
    "ReversedNormalXVA",
    ["log.txt", "xva.csv"]
    + glob.glob(os.path.join(os.getcwd(), os.path.join("Output", "exposure*")))
)

#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE for Sensitivity and Stress Analysis")
oreex.run("Input/ore_stress.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Run ORE for ParStressTestConversion Analysis")
oreex.run("Input/ore_parstressconversion.xml")

oreex.print_headline("Run ORE for Shifted CapFloor Surface NPV Analysis")
oreex.run("Input/ore_valid_cap.xml")

oreex.print_headline("Run ORE for Shifted Credit Curve NPV Analysis")
oreex.run("Input/ore_valid_cds.xml")

oreex.print_headline("Run ORE for Shifted X-CCY Basis Curve NPV Analysis")
oreex.run("Input/ore_valid_xccy.xml")

oreex.print_headline("Run ORE for Shifted EUR ESTER Curve NPV Analysis")
oreex.run("Input/ore_valid_ester.xml")

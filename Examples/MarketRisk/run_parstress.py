#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------+")
print("| Stress Test in the par domain          |")
print("+----------------------------------------+")

# legacy example 63

oreex.print_headline("Run ORE for Stress testing in the par domain")
oreex.run("Input/ore_parstress.xml")

oreex.print_headline("Run ORE for ParStressTestConversion Analysis")
oreex.run("Input/ore_parstressconversion.xml")

oreex.print_headline("Run ORE for Shifted CapFloor Surface NPV Analysis")
oreex.run("Input/ore_parstress_cap.xml")

oreex.print_headline("Run ORE for Shifted Credit Curve NPV Analysis")
oreex.run("Input/ore_parstress_cds.xml")

oreex.print_headline("Run ORE for Shifted X-CCY Basis Curve NPV Analysis")
oreex.run("Input/ore_parstress_xccy.xml")

oreex.print_headline("Run ORE for Shifted EUR ESTER Curve NPV Analysis")
oreex.run("Input/ore_parstress_ester.xml")

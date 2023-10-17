#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

orexmls = [
	("Input/ore_SIMM2.4_10D.xml", "2.4", "10"),
	("Input/ore_SIMM2.4_1D.xml", "2.4", "1"),
	("Input/ore_SIMM2.6_10D.xml", "2.6", "10"),
	("Input/ore_SIMM2.6_1D.xml", "2.6", "1")
]
for orexml in orexmls:
	oreex.print_headline(f"Run ORE SIMM; version={orexml[1]}; MPOR days={orexml[2]}")
	oreex.run(orexml[0])

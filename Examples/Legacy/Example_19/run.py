#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE with flat vols")
oreex.run("Input/ore_flat.xml")

oreex.print_headline("Run ORE with smiles")
oreex.run("Input/ore_smile.xml")

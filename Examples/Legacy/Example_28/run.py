#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to illustrate use of discount ratio curve")

oreex.print_headline("Run with USD base currency")
oreex.run("Input/ore_usd_base.xml")

oreex.print_headline("Run with EUR base currency using GBP-IN-EUR discount modified ratio curve")
oreex.run("Input/ore_eur_base.xml")

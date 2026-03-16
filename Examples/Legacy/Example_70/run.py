#!/usr/bin/env python

import sys
import os
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE to produce XVA Explain with AMC")
oreex.run("Input/ore.xml")


'''
 Copyright (C) 2018-2023 Quaternion Risk Management Ltd

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org
 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>
 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
'''

import sys, time
from ORE import *

xml = "Input/ore.xml"
params = Parameters()
params.fromFile(xml)
ore = OREApp(params)
print ("Run ORE for", xml)
ore.run()

xml = "Input/ore_threshold.xml"
params = Parameters()
params.fromFile(xml)
ore_threshold = OREApp(params)
print ("Run ORE for", xml)
ore_threshold.run()

xml = "Input/ore_mta.xml"
params = Parameters()
params.fromFile(xml)
ore_mta = OREApp(params)
print ("Run ORE for", xml)
ore_mta.run()

xml = "Input/ore_mpor.xml"
params = Parameters()
params.fromFile(xml)
ore_mpor = OREApp(params)
print ("Run ORE for", xml)
ore_mpor.run()

print ("ORE process done")

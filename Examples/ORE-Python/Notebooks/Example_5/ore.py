# Copyright (C) 2018 Quaternion Risk Manaement Ltd
# All rights reserved.

from ORE import *

print ("Loading parameters...")
params = Parameters()
print ("   params is of type", type(params))
params.fromFile("Input/ore.xml")
print ("   setup/asofdate = " + params.get("setup","asofDate"))

print ("Building ORE App...")
ore = OREApp(params)
print ("   ore is of type", type(ore))

print ("Running ORE process...");
# Run it all 
ore.run()

print("Done")

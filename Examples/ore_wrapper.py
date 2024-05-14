"""
 Copyright (C) 2023 Quaternion Risk Management Ltd
 All rights reserved.
"""

import os
import argparse
import ORE

if __name__ == "__main__":
    print("ORE Python Interface")
    parser = argparse.ArgumentParser(prog="ORE Python interface")
    parser.add_argument('path_to_ore_xml', default=".\Input\ore.xml")
    args = parser.parse_args()
    print ("Loading parameters...")
    params = ORE.Parameters()
    params.fromFile(args.path_to_ore_xml)
    print ("Creating OREApp...")
    ore = ORE.OREApp(params, True)
    print ("Running ORE process...");
    ore.run()
    print ("Running ORE process done");

#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposure: Inflation DK                           |")
print("+--------------------------------------------------+")

# Originally legacy example 17, but now using the setup of legacy 32
# with the only change in the simulation model setup.

oreex.run("Input/ore_inflation_dk.xml")
oreex.setup_plot("exposure_inflation_dk")
oreex.plot("inflation_dk/exposure_trade_trade_01.csv", 2, 3, 'r', "EPE CPI Swap 1")
oreex.plot("inflation_dk/exposure_trade_trade_02.csv", 2, 3, 'g', "EPE CPI Swap 2")
oreex.plot("inflation_dk/exposure_trade_trade_03.csv", 2, 3, 'b', "EPE YOY Swap 1")
oreex.plot("inflation_dk/exposure_trade_trade_04.csv", 2, 3, 'y', "EPE YOY Swap 2")
oreex.decorate_plot(title="Inflation Swap Exposure, Dodgson-Kainth")
oreex.save_plot_to_file()

print("+--------------------------------------------------+")
print("| Exposure: Inflation JY                           |")
print("+--------------------------------------------------+")

# This is legacy example 32

oreex.run("Input/ore_inflation_jy.xml")
oreex.setup_plot("exposure_inflation_jy")
oreex.plot("inflation_jy/exposure_trade_trade_01.csv", 2, 3, 'r', "EPE CPI Swap 1")
oreex.plot("inflation_jy/exposure_trade_trade_02.csv", 2, 3, 'g', "EPE CPI Swap 2")
oreex.plot("inflation_jy/exposure_trade_trade_03.csv", 2, 3, 'b', "EPE YOY Swap 1")
oreex.plot("inflation_jy/exposure_trade_trade_04.csv", 2, 3, 'y', "EPE YOY Swap 2")
oreex.decorate_plot(title="Inflation Swap Exposure, Jarrow-Yildirim")
oreex.save_plot_to_file()




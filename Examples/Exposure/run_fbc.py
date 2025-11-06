#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposure: Formula-based Coupon                   |")
print("+--------------------------------------------------+")

# Legacy example 64

oreex.print_headline("Run ORE for formula based coupon examples")
oreex.run("Input/ore_fbc.xml")

oreex.print_headline("Plot results")

oreex.setup_plot("exposure_fbc_cmsspread")
oreex.plot("fbc/exposure_trade_SWAP_EUR_CMSSpread_fbc.csv", 2, 3, 'r', "EPE Swap with CMS Spread Leg FBC")
oreex.plot("fbc/exposure_trade_SWAP_EUR_CMSSpread_analytic.csv", 2, 3, 'b', "EPE Swap with CMS Spread Leg Analytical")
oreex.decorate_plot(title="'Swap with CMS spread leg: formula based coupon vs analytical pricer")
oreex.save_plot_to_file()


oreex.setup_plot("exposure_fbc_digitalcmsspread")
oreex.plot("fbc/exposure_trade_SWAP_EUR_DigitalCMSSpread_fbc.csv", 2, 3, 'r', "EPE Swap with Digital CMS Spread Leg FBC")
oreex.plot("fbc/exposure_trade_SWAP_EUR_DigitalCMSSpread_analytic.csv", 2, 3, 'g', "EPE Swap with Digital CMS Spread Leg Analytical")
oreex.decorate_plot(title="'Swap with Digital CMS spread leg: formula based coupon vs analytical pricer")
oreex.save_plot_to_file()

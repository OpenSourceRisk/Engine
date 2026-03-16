#!/usr/bin/env python

import sys
sys.path.append('../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE for formula based coupon examples")
oreex.run("Input/ore.xml")
oreex.get_times("Output/log.txt")

oreex.print_headline("Plot results")

oreex.setup_plot("CMSSpread")
oreex.plot("exposure_trade_SWAP_EUR_CMSSpread_fbc.csv", 2, 3, 'r', "EPE Swap with CMSSpreadLeg FBC")
oreex.plot("exposure_trade_SWAP_EUR_CMSSpread_analytic.csv", 2, 3, 'b', "EPE Swap with CMSSpreadLeg Analytical ")
oreex.decorate_plot(title="'Swap with CMS spread leg: formula based coupon vs analytical pricer")
oreex.save_plot_to_file()


oreex.setup_plot("DigitalCMSSpread")
oreex.plot("exposure_trade_SWAP_EUR_DigitalCMSSpread_fbc.csv", 2, 3, 'r', "EPE Swap with DigitalCMSSpreadLeg FBC")
oreex.plot("exposure_trade_SWAP_EUR_DigitalCMSSpread_analytic.csv", 2, 3, 'g', "EPE Swap with DigitalCMSSpreadLeg Analytical ")
oreex.decorate_plot(title="'Swap with Digital CMS spread leg: formula based coupon vs analytical pricer")
oreex.save_plot_to_file()

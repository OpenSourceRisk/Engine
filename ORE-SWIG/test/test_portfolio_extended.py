"""
 Copyright (C) 2026 Quaternion Risk Management Ltd
 All rights reserved.
"""

from ORE import *
import unittest
import tempfile
import os


class ExtendedPortfolioBindingsTest(unittest.TestCase):

    def test_new_trade_classes_constructible(self):
        self.assertIsNotNone(ForwardBond())
        self.assertIsNotNone(BondOption())
        self.assertIsNotNone(TRS())
        self.assertTrue(hasattr(FxDoubleBarrierOption, "fromXML"))
        self.assertTrue(hasattr(FxEuropeanBarrierOption, "fromXML"))
        self.assertIsNotNone(CommodityDigitalOption())
        self.assertIsNotNone(CommoditySpreadOption())
        self.assertIsNotNone(CommodityAveragePriceOption())
        self.assertIsNotNone(ScriptedTrade())

    def test_new_leg_classes_constructible(self):
        self.assertIsNotNone(CMSSpreadLegData())
        self.assertIsNotNone(DigitalCMSSpreadLegData())
        self.assertIsNotNone(EquityLegData())

    def test_portfolio_from_file(self):
        p1 = Portfolio()
        xml = p1.toXMLString()

        fd, path = tempfile.mkstemp(suffix=".xml")
        try:
            os.close(fd)
            with open(path, "w", encoding="utf-8") as handle:
                handle.write(xml)

            p2 = Portfolio()
            p2.fromFile(path)
            self.assertEqual(p2.size(), 0)
        finally:
            if os.path.exists(path):
                os.remove(path)


if __name__ == '__main__':
    print('testing ORE extended portfolio bindings')
    unittest.main()

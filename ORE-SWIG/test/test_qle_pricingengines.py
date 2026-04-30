"""
Copyright (C) 2026 Quaternion Risk Management Ltd
All rights reserved.
"""

from ORE import *
import unittest


class QlePricingEnginesSmokeTest(unittest.TestCase):
    def setUp(self):
        """Set up common market handles for engine construction smoke tests."""
        self.today = Date(2, March, 2026)
        Settings.instance().evaluationDate = self.today
        self.day_counter = Actual365Fixed()
        self.discount_curve = YieldTermStructureHandle(
            FlatForward(self.today, 0.02, self.day_counter)
        )
        self.vol = BlackVolTermStructureHandle(
            BlackConstantVol(self.today, TARGET(), 0.20, self.day_counter)
        )

    def test_class_exports(self):
        """Verify Task 4 pricing engine classes are exported."""
        expected = [
            "AnalyticLgmSwaptionEngine",
            "McMultiLegOptionEngine",
            "NumericLgmMultiLegOptionEngine",
            "CommodityAveragePriceOptionAnalyticalEngine",
            "CommoditySwaptionEngine",
            "DiscountingBondTRSEngine",
            "BlackIndexCdsOptionEngine",
            "DiscountingSwapEngineDeltaGamma",
        ]
        for class_name in expected:
            self.assertTrue(hasattr(__import__("ORE"), class_name), class_name)

    def test_construct_simple_engines(self):
        """Construct engines with simple signatures to validate wrappers."""
        dse = DiscountingSwapEngineDeltaGamma(self.discount_curve)
        self.assertIsNotNone(dse)

        btrs = DiscountingBondTRSEngine(self.discount_curve, False, False)
        self.assertIsNotNone(btrs)

        cs = CommoditySwaptionEngine(self.discount_curve, self.vol, 0.0)
        self.assertIsNotNone(cs)

        apo = CommodityAveragePriceOptionAnalyticalEngine(
            self.discount_curve,
            self.vol,
            0.0,
        )
        self.assertIsNotNone(apo)


if __name__ == "__main__":
    unittest.main()

"""Smoke tests for OREAnalytics high-level analytic wrappers."""

import unittest

import ORE


class AnalyticsBindingSmokeTest(unittest.TestCase):
    """Validate Task 8 analytics and cube symbols."""

    def test_analytics_symbols_available(self) -> None:
        """Assert analytic orchestration classes are exported."""
        required = [
            "AnalyticFactory",
            "PricingAnalytic",
            "XvaAnalytic",
            "SimmAnalytic",
            "SaccrAnalytic",
            "SensitivityCube",
            "CubeWriter",
            "CubeReader",
        ]
        for name in required:
            self.assertTrue(hasattr(ORE, name), msg=f"Missing ORE symbol: {name}")

    def test_construct_core_analytics(self) -> None:
        """Construct analytics with default InputParameters wrappers."""
        pricing = ORE.PricingAnalytic()
        xva = ORE.XvaAnalytic()
        saccr = ORE.SaccrAnalytic()

        self.assertIsNotNone(pricing)
        self.assertIsNotNone(xva)
        self.assertIsNotNone(saccr)


if __name__ == "__main__":
    unittest.main()

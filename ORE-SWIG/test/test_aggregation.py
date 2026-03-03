"""Smoke tests for OREAnalytics aggregation bindings."""

import unittest

import ORE


class AggregationBindingSmokeTest(unittest.TestCase):
    """Validate presence of Task 7 aggregation symbols."""

    def test_aggregation_symbols_available(self) -> None:
        """Assert that major aggregation-layer types are wrapped."""
        required = [
            "PostProcess",
            "ExposureCalculator",
            "DynamicInitialMarginCalculator",
            "CollateralAccount",
            "XvaCalculator",
        ]
        for name in required:
            self.assertTrue(hasattr(ORE, name), msg=f"Missing ORE symbol: {name}")

    def test_collateral_account_constructs(self) -> None:
        """Construct default collateral account."""
        account = ORE.CollateralAccount()
        self.assertIsNotNone(account)


if __name__ == "__main__":
    unittest.main()

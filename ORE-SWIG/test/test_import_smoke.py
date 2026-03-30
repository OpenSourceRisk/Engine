"""
Basic smoke tests for ORE Python bindings.
"""

import unittest

import ORE


class ImportSmokeTest(unittest.TestCase):
    """Verify key wrapped symbols are importable and minimally usable."""

    def test_core_symbols_are_available(self) -> None:
        """Validate that representative symbols from each layer are exposed."""
        required_symbols = [
            "Date",
            "Settings",
            "CrossCcyBasisSwap",
            "InMemoryLoader",
            "Wildcard",
            "Portfolio",
            "InputParameters",
            "OREApp",
            "NPVCube",
            "YieldCurveType_Discount",
        ]
        for symbol in required_symbols:
            self.assertTrue(
                hasattr(ORE, symbol),
                msg=f"Missing symbol in ORE module: {symbol}",
            )

    def test_market_surface_symbols_are_available(self) -> None:
        """Verify the targeted market wrapper surface is exported."""
        required_methods = [
            "fxRate",
            "capFloorVolIndexBase",
            "conversionFactor",
            "securityPrice",
            "commodityIndex",
        ]

        for method in required_methods:
            self.assertTrue(
                hasattr(ORE.MarketImpl, method),
                msg=f"Missing MarketImpl method in ORE module: {method}",
            )

    def test_minimal_object_construction(self) -> None:
        """Construct lightweight objects to confirm wrappers are functional."""
        today = ORE.Date(2, ORE.March, 2026)
        ORE.Settings.instance().evaluationDate = today
        loader = ORE.InMemoryLoader()
        portfolio = ORE.Portfolio()

        self.assertEqual(str(ORE.Settings.instance().evaluationDate), str(today))
        self.assertIsNotNone(loader)
        self.assertIsNotNone(portfolio)


if __name__ == "__main__":
    unittest.main()

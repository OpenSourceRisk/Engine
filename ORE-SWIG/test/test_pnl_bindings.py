"""Smoke tests for PnL explain and analytics SWIG bindings."""

import unittest

import ORE


class TestPnlBindings(unittest.TestCase):
    """Validate core PnL binding symbols and simple construction paths."""

    def test_pnl_results_struct(self):
        """PnlExplainResults is exposed with all expected fields."""
        results_fields = [
            "riskFactor",
            "pnl",
            "delta",
            "gamma",
            "vega",
            "irDelta",
            "irGamma",
            "irVega",
            "eqDelta",
            "eqGamma",
            "eqVega",
            "fxDelta",
            "fxGamma",
            "fxVega",
            "infDelta",
            "infGamma",
            "infVega",
            "creditDelta",
            "creditGamma",
            "creditVega",
            "comDelta",
            "comGamma",
            "comVega",
        ]

        self.assertTrue(
            hasattr(ORE, "PnlExplainResults"), "PnlExplainResults not found in ORE"
        )
        result = ORE.PnlExplainResults()
        for field in results_fields:
            self.assertTrue(
                hasattr(result, field),
                msg=f"Field '{field}' missing from PnlExplainResults",
            )

    def test_pnl_analytic(self):
        """PnlAnalytic is exposed and constructible with default constructor."""
        self.assertTrue(hasattr(ORE, "PnlAnalytic"), "PnlAnalytic not found in ORE")
        pnl_analytic = ORE.PnlAnalytic()
        self.assertIsInstance(pnl_analytic, ORE.Analytic)

    def test_pnl_explain_analytic(self):
        """PnlExplainAnalytic is exposed and constructible with default constructor."""
        self.assertTrue(
            hasattr(ORE, "PnlExplainAnalytic"), "PnlExplainAnalytic not found in ORE"
        )
        pnl_explain_analytic = ORE.PnlExplainAnalytic()
        self.assertIsInstance(pnl_explain_analytic, ORE.Analytic)

    def test_downcast_helpers(self):
        """Downcast helper symbols are exported."""
        self.assertTrue(hasattr(ORE, "asPnlAnalytic"), "asPnlAnalytic not found")
        self.assertTrue(
            hasattr(ORE, "asPnlExplainAnalytic"), "asPnlExplainAnalytic not found"
        )

    def test_historical_pnl_generator_shape(self):
        """HistoricalPnlGenerator and key methods are visible."""
        self.assertTrue(
            hasattr(ORE, "HistoricalPnlGenerator"),
            "HistoricalPnlGenerator not found in ORE",
        )
        pnl_gen_class = ORE.HistoricalPnlGenerator
        for method in ["generateCube", "pnl", "tradeLevelPnl", "cube"]:
            self.assertTrue(
                hasattr(pnl_gen_class, method),
                msg=f"Method '{method}' not found on HistoricalPnlGenerator",
            )
        self.assertTrue(
            hasattr(ORE, "DoubleVectorVector"),
            "DoubleVectorVector template not found",
        )


if __name__ == "__main__":
    unittest.main()

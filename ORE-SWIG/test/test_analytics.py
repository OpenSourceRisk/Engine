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


class HistoricalSimulationVarBindingTest(unittest.TestCase):
    """Validate the public historical simulation VaR SWIG workflow."""

    def test_analytic_construction_and_downcast(self) -> None:
        """Construct the analytic through both supported public constructors."""
        self.assertTrue(
            hasattr(ORE, "HistoricalSimulationVarAnalytic"),
            msg="Missing ORE symbol: HistoricalSimulationVarAnalytic",
        )
        self.assertTrue(
            hasattr(ORE, "asHistoricalSimulationVarAnalytic"),
            msg="Missing ORE symbol: asHistoricalSimulationVarAnalytic",
        )

        default_analytic = ORE.HistoricalSimulationVarAnalytic()
        configured_analytic = ORE.HistoricalSimulationVarAnalytic(
            ORE.InputParameters()
        )

        self.assertIsInstance(default_analytic, ORE.Analytic)
        self.assertIsInstance(configured_analytic, ORE.Analytic)
        self.assertIsInstance(
            ORE.asHistoricalSimulationVarAnalytic(default_analytic),
            ORE.HistoricalSimulationVarAnalytic,
        )

    def test_analytic_owned_named_report(self) -> None:
        """Retrieve an installed report by its analytic key and sub-key."""
        analytic = ORE.HistoricalSimulationVarAnalytic()
        report = ORE.InMemoryReport()
        report.addColumnReal("VaR")
        report.nextRow()
        report.addReal(-12.5)
        report.end()

        analytic.addReport("HISTSIM_VAR", "var", report)
        actual = analytic.getReport("HISTSIM_VAR", "var")

        self.assertEqual(actual.rows(), 1)
        self.assertEqual(actual.header(0), "VaR")

    def test_direct_calculator_preserves_tail_pnl_sign(self) -> None:
        """Calculate tail P&L and reverse its sign with isCall=False."""
        self.assertTrue(
            hasattr(ORE, "HistoricalSimulationVarCalculator"),
            msg="Missing ORE symbol: HistoricalSimulationVarCalculator",
        )
        calculator = ORE.HistoricalSimulationVarCalculator(
            [float(i) for i in range(1, 1002)]
        )

        value_at_risk = calculator.var(0.8)
        expected_shortfall = calculator.expectedShortfall(0.8)
        reversed_value_at_risk = calculator.var(0.8, False)
        reversed_expected_shortfall = calculator.expectedShortfall(0.8, False)
        trade_ids = [("trade", 0)]

        self.assertEqual(value_at_risk, 801.0)
        self.assertEqual(expected_shortfall, 401.0)
        self.assertEqual(reversed_value_at_risk, -201.0)
        self.assertEqual(reversed_expected_shortfall, -601.0)
        self.assertEqual(calculator.var(0.8, True, trade_ids), value_at_risk)
        self.assertEqual(
            calculator.expectedShortfall(0.8, True, trade_ids), expected_shortfall
        )


if __name__ == "__main__":
    unittest.main()

"""Smoke tests for OREAnalytics scenario and simulation bindings."""

import unittest

import ORE


class ScenarioBindingSmokeTest(unittest.TestCase):
    """Verify core scenario and simulation symbols are exposed."""

    def test_scenario_symbols_available(self) -> None:
        """Assert that newly wrapped scenario-layer symbols exist."""
        required = [
            "RiskFactorKey",
            "Scenario",
            "SimpleScenario",
            "ScenarioFactory",
            "CloneScenarioFactory",
            "ScenarioGenerator",
            "StaticScenarioGenerator",
            "ScenarioGeneratorData",
            "ScenarioSimMarket",
            "FixingManager",
        ]
        for name in required:
            self.assertTrue(hasattr(ORE, name), msg=f"Missing ORE symbol: {name}")

    def test_minimal_constructs(self) -> None:
        """Construct lightweight concrete objects where possible."""
        today = ORE.Date(3, ORE.March, 2026)
        key = ORE.RiskFactorKey()
        key.name = "EURUSD"
        key.index = 0
        data = ORE.ScenarioGeneratorData()
        fixing_manager = ORE.FixingManager(today)

        self.assertEqual(key.name, "EURUSD")
        self.assertIsNotNone(data)
        self.assertIsNotNone(fixing_manager)


if __name__ == "__main__":
    unittest.main()

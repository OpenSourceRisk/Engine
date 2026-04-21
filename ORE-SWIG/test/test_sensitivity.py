"""Smoke tests for OREAnalytics engine bindings."""

import unittest

import ORE


class EngineBindingSmokeTest(unittest.TestCase):
    """Validate presence of Task 7 engine symbols."""

    def test_engine_symbols_available(self) -> None:
        """Assert that key engine classes/functions are available."""
        required = [
            "SensitivityAnalysis",
            "ParSensitivityAnalysis",
            "ValuationEngine",
            "VarCalculator",
            "ParametricVarCalculator",
            "runStressTest",
        ]
        for name in required:
            self.assertTrue(hasattr(ORE, name), msg=f"Missing ORE symbol: {name}")


if __name__ == "__main__":
    unittest.main()

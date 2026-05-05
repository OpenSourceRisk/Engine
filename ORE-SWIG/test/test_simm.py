"""Smoke tests for OREAnalytics SIMM bindings."""

import unittest

import ORE


class SimmBindingSmokeTest(unittest.TestCase):
    """Validate Task 8 SIMM symbols and basic construction paths."""

    def test_simm_symbols_available(self) -> None:
        """Assert SIMM binding symbols are exported."""
        required = [
            "CrifRecord",
            "Crif",
            "SimmBucketMapper",
            "SimmBucketMapperBase",
            "SimmConfiguration",
            "SimmConfigurationBase",
            "SimmConfiguration_ISDA_V2_6",
            "SimmResults",
            "CrifLoader",
            "CsvFileCrifLoader",
            "CsvBufferCrifLoader",
            "SimmCalculator",
        ]
        for name in required:
            self.assertTrue(hasattr(ORE, name), msg=f"Missing ORE symbol: {name}")

    def test_minimal_simm_construction(self) -> None:
        """Construct mapper, config, CRIF and calculator."""
        config = ORE.SimmConfiguration_ISDA_V2_6()
        crif = ORE.Crif()
        calc = ORE.SimmCalculator()

        self.assertIsNotNone(config)
        self.assertIsNotNone(crif)
        self.assertIsNotNone(calc)


if __name__ == "__main__":
    unittest.main()

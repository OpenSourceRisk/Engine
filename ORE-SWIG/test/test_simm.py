"""Smoke tests for OREAnalytics SIMM bindings."""

import unittest

import ORE

# Minimal tab-delimited CRIF with one IRCurve delta record in USD.
# Uses PortfolioID as the netting set identifier.
_CRIF_BUFFER = (
    "TradeID\tPortfolioID\tProductClass\tRiskType\t"
    "Qualifier\tBucket\tLabel1\tLabel2\t"
    "AmountCurrency\tAmount\tcollect_regulations\tpost_regulations\n"
    "trade1\tNS_TEST\tRatesFX\tRisk_IRCurve\t"
    "USD\t1\t2y\tLibor3m\t"
    "USD\t-1000000.0\t\t\n"
)


class SimmBindingSmokeTest(unittest.TestCase):
    """Validate SIMM symbols and basic construction paths."""

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
            "RegulationSimmResultsPair",
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


class SimmCalculatorResultAccessorTest(unittest.TestCase):
    """Validate ACADIAQPR-14131: SimmCalculator result accessor methods."""

    def test_result_accessor_methods_present(self) -> None:
        """All four result accessor methods must be present on SimmCalculator."""
        calc = ORE.SimmCalculator()
        for method in ("simmResults", "finalSimmResults", "winningRegulations", "simmParameters"):
            self.assertTrue(hasattr(calc, method), msg=f"Missing SimmCalculator method: {method}")

    def test_simm_parameters_empty_crif(self) -> None:
        """simmParameters() returns a Crif object even for an empty calculation."""
        calc = ORE.SimmCalculator()
        params = calc.simmParameters()
        self.assertIsNotNone(params)

    def test_full_load_calculate_retrieve_workflow(self) -> None:
        """Full workflow: load CRIF via buffer, run calculator, retrieve margins.

        Demonstrates the complete SIMM usage pattern:
        load → calculate → winningRegulations → simmResults → finalSimmResults.
        """
        config = ORE.SimmConfiguration_ISDA_V2_6()
        loader = ORE.CsvBufferCrifLoader(_CRIF_BUFFER, config)
        crif = loader.loadCrif()
        self.assertFalse(crif.empty(), "Loaded CRIF should contain records")

        calc = ORE.SimmCalculator(crif, config)

        # simmParameters holds any SIMM parameter records extracted during calculation
        params = calc.simmParameters()
        self.assertIsNotNone(params)

        nsd = ORE.NettingSetDetails("NS_TEST")
        call_side = ORE.SimmConfiguration.SimmSide_Call

        # Retrieve the winning regulation for the netting set on the Call side
        winning_reg = calc.winningRegulations(call_side, nsd)

        # Query detailed SIMM results for that regulation
        reg_set = ORE.RegulationSet()
        reg_set.insert(winning_reg)
        results = calc.simmResults(call_side, nsd, reg_set)
        self.assertIsNotNone(results)
        self.assertIsInstance(results.resultCurrency(), str)

        # finalSimmResults returns a (Regulation, SimmResults) pair
        final = calc.finalSimmResults(call_side, nsd)
        self.assertIsNotNone(final)
        self.assertIsNotNone(final.first)   # winning Regulation enum value
        self.assertIsNotNone(final.second)  # SimmResults object
        self.assertIsInstance(final.second.resultCurrency(), str)


if __name__ == "__main__":
    unittest.main()

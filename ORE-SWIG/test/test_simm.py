"""Tests for OREAnalytics SIMM bindings (ACADIAQPR-14131).

Covers Ticket 1: SimmCalculator result accessor methods (simmResults,
finalSimmResults, winningRegulations, simmParameters) and Ticket 2:
CrifRecord data members and Crif iteration.
"""

import unittest

import ORE

# ---------------------------------------------------------------------------
# Tab-delimited CRIF buffer (tab is the default delimiter for CsvBufferCrifLoader).
# Risk_IRCurve is the canonical risk-type string (bimap in crifrecord.cpp).
# AmountUSD is required by SimmCalculator when no market object is provided;
# without it the calculator tries FX conversion via a null market and throws.
#
# collect_regulations / post_regulations are intentionally omitted: the CSV
# loader uses boost::trim on each line, which strips trailing tabs, and those
# two columns are always empty for this test record.  When present, the trimmed
# row has fewer entries than the maxIndex derived from the header, causing the
# loader to silently reject the record.  These columns are optional.
# ---------------------------------------------------------------------------
_CRIF_BUFFER = (
    "TradeID\tPortfolioID\tProductClass\tRiskType\t"
    "Qualifier\tBucket\tLabel1\tLabel2\t"
    "AmountCurrency\tAmount\tAmountUSD\n"
    "trade1\tNS_TEST\tRatesFX\tRisk_IRCurve\t"
    "USD\t1\t2y\tLibor3m\t"
    "USD\t-1000000.0\t-1000000.0\n"
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
    """Validate ACADIAQPR-14131 Ticket 1: SimmCalculator result accessor methods."""

    def test_result_accessor_methods_present(self) -> None:
        """All four result accessor methods must be present on SimmCalculator."""
        calc = ORE.SimmCalculator()
        for method in ("simmResults", "finalSimmResults", "winningRegulations", "simmParameters"):
            self.assertTrue(hasattr(calc, method), msg=f"Missing SimmCalculator method: {method}")

    def test_accessor_methods_callable(self) -> None:
        """All result accessor methods must be callable Python callables."""
        calc = ORE.SimmCalculator()
        for method in ("simmResults", "finalSimmResults", "winningRegulations", "simmParameters"):
            self.assertTrue(callable(getattr(calc, method, None)),
                            msg=f"SimmCalculator.{method} is not callable")

    def test_simm_parameters_is_none_without_param_records(self) -> None:
        """simmParameters() correctly returns None when the CRIF has no SIMM parameter records.

        The C++ member simmParameters_ is initialised to null and is only
        populated when records with RiskType in {AddOnFixedAmount,
        AddOnNotionalFactor, ProductClassMultiplier} are encountered during
        calculation.  A standard IRCurve sensitivity CRIF never sets it, so
        simmParameters() returns None — this is the correct C++ behaviour and
        the Python binding must faithfully reflect it.
        """
        config = ORE.SimmConfiguration_ISDA_V2_6()
        loader = ORE.CsvBufferCrifLoader(_CRIF_BUFFER, config)
        crif = loader.loadCrif()
        self.assertFalse(crif.empty(), "Loaded CRIF must contain records")
        calc = ORE.SimmCalculator(crif, config)
        # No parameter records → simmParameters_ stays null → Python sees None
        self.assertIsNone(calc.simmParameters())

    def test_simm_parameters_returns_crif_for_param_records(self) -> None:
        """simmParameters() returns a non-None Crif when the CRIF contains SIMM parameter records.

        ProductClassMultiplier is a SIMM parameter record type (isSimmParameter()
        == true).  However, SimmCalculator::calcAddMargin() — the only place that
        populates simmParameters_ — is only reached from calculateRegulationSimm(),
        which is only called when crif.hasCrifRecords() is true.
        hasCrifRecords() returns true only when there is at least one record that
        is NOT a SIMM parameter.  Therefore the CRIF must contain both:
          1. A regular sensitivity record (IRCurve), so the calculation proceeds.
          2. A ProductClassMultiplier record, which calcAddMargin() extracts and
             stores in simmParameters_.
        """
        config = ORE.SimmConfiguration_ISDA_V2_6()
        nsd = ORE.NettingSetDetails("NS_PARAM")

        # Regular IRCurve sensitivity — makes hasCrifRecords() return True so
        # calculateRegulationSimm → calcAddMargin is actually executed.
        # AmountUSD is required; without it the calculator attempts FX conversion
        # via a null market and throws.
        ir_rec = ORE.CrifRecord(
            "trade_p1",                             # tradeId
            "Swap",                                 # tradeType
            nsd,                                    # nettingSetDetails
            ORE.CrifRecord.ProductClass_RatesFX,    # productClass
            ORE.CrifRecord.RiskType_IRCurve,        # riskType
            "USD",                                  # qualifier
            "1",                                    # bucket
            "2y",                                   # label1
            "Libor3m",                              # label2
            "USD",                                  # amountCurrency
            -1000000.0,                             # amount
            -1000000.0,                             # amountUsd (required)
        )

        # ProductClassMultiplier: productClass must be Empty, qualifier must
        # parse as a valid ProductClass string, amount >= 0.
        # Does NOT need amountUsd (requiresAmountUsd() returns false for this type).
        param_rec = ORE.CrifRecord(
            "",                                               # tradeId
            "",                                               # tradeType
            nsd,                                              # nettingSetDetails
            ORE.CrifRecord.ProductClass_Empty,                # productClass (required)
            ORE.CrifRecord.RiskType_ProductClassMultiplier,   # riskType
            "RatesFX",                                        # qualifier (valid ProductClass)
            "",                                               # bucket
            "",                                               # label1
            "",                                               # label2
            "",                                               # amountCurrency
            1.0,                                              # amount (>= 0 required)
            0.0,                                              # amountUsd
        )

        crif = ORE.Crif()
        crif.addRecord(ir_rec)
        crif.addRecord(param_rec)
        self.assertFalse(crif.empty())
        self.assertTrue(crif.hasCrifRecords(), "Crif must have a non-param record for calcAddMargin to be reached")
        self.assertTrue(crif.hasSimmParameters(), "Crif must have a SIMM param record for simmParameters_ to be set")

        calc = ORE.SimmCalculator(crif, config)
        params = calc.simmParameters()
        self.assertIsNotNone(params, "simmParameters() must return a Crif when param records exist")
        self.assertIsInstance(params, ORE.Crif)

    def test_full_workflow_load_calculate_retrieve(self) -> None:
        """Full workflow: load CRIF, run SimmCalculator, query all result accessors.

        Demonstrates the canonical usage pattern:
          CsvBufferCrifLoader → loadCrif → SimmCalculator →
          winningRegulations → simmResults → finalSimmResults
        """
        config = ORE.SimmConfiguration_ISDA_V2_6()
        loader = ORE.CsvBufferCrifLoader(_CRIF_BUFFER, config)
        crif = loader.loadCrif()
        self.assertFalse(crif.empty(), "Loaded CRIF must contain records")

        calc = ORE.SimmCalculator(crif, config)
        nsd = ORE.NettingSetDetails("NS_TEST")
        call_side = ORE.SimmConfiguration.SimmSide_Call

        # winningRegulations: returns the winning Regulation enum value
        winning_reg = calc.winningRegulations(call_side, nsd)
        self.assertIsNotNone(winning_reg)

        # simmResults: query using the single-regulation convenience overload.
        # The set-based overload simmResults(side, nsd, RegulationSet) is also
        # available but constructing RegulationSet from a Python int (the SWIG
        # enum value) requires the single-value overload added via %extend.
        results = calc.simmResults(call_side, nsd, winning_reg)
        self.assertIsNotNone(results)
        # resultCurrency() has both mutable (std::string&) and const overloads;
        # SWIG returns the mutable overload as a proxy — use assertIsNotNone.
        self.assertIsNotNone(results.resultCurrency())

        # finalSimmResults: returns a (Regulation, SimmResults) pair
        final = calc.finalSimmResults(call_side, nsd)
        self.assertIsNotNone(final)
        self.assertIsNotNone(final.first)   # winning Regulation enum value
        self.assertIsNotNone(final.second)  # SimmResults object
        self.assertIsNotNone(final.second.resultCurrency())


class CrifRecordDataMemberTest(unittest.TestCase):
    """Validate ACADIAQPR-14131 Ticket 2: CrifRecord data members and Crif iteration."""

    def test_crifrec_vector_symbol_available(self) -> None:
        """CrifRecordVector template instantiation must be exported."""
        self.assertTrue(hasattr(ORE, "CrifRecordVector"))

    def test_crifrecord_construction_and_field_access(self) -> None:
        """Construct a CrifRecord with known fields and read them back."""
        nsd = ORE.NettingSetDetails("NS1")
        rec = ORE.CrifRecord(
            "trade_A",                              # tradeId
            "Swap",                                 # tradeType
            nsd,                                    # nettingSetDetails
            ORE.CrifRecord.ProductClass_RatesFX,    # productClass
            ORE.CrifRecord.RiskType_IRCurve,        # riskType
            "USD",                                  # qualifier
            "1",                                    # bucket
            "2y",                                   # label1
            "Libor3m",                              # label2
            "USD",                                  # amountCurrency
            -500000.0,                              # amount
            -500000.0,                              # amountUsd
        )
        # String fields
        self.assertEqual(rec.tradeId, "trade_A")
        self.assertEqual(rec.tradeType, "Swap")
        self.assertEqual(rec.qualifier, "USD")
        self.assertEqual(rec.bucket, "1")
        self.assertEqual(rec.label1, "2y")
        self.assertEqual(rec.label2, "Libor3m")
        self.assertEqual(rec.amountCurrency, "USD")
        # Numeric fields
        self.assertAlmostEqual(rec.amount, -500000.0)
        self.assertAlmostEqual(rec.amountUsd, -500000.0)
        # Enum fields: SWIG enum class values are Python ints, compare directly
        self.assertEqual(rec.riskType, ORE.CrifRecord.RiskType_IRCurve)
        self.assertEqual(rec.productClass, ORE.CrifRecord.ProductClass_RatesFX)
        # NettingSetDetails field is accessible as a SWIG proxy object
        self.assertIsNotNone(rec.nettingSetDetails)
        # collectRegulations and postRegulations are both empty by default
        self.assertEqual(rec.collectRegulations.size(), 0)
        self.assertEqual(rec.postRegulations.size(), 0)
        # Query methods
        self.assertFalse(rec.isEmpty())
        self.assertTrue(rec.hasAmount())
        self.assertTrue(rec.hasAmountUsd())
        self.assertFalse(rec.isSimmParameter(), "IRCurve is not a SIMM parameter record")
        self.assertTrue(rec.requiresAmountUsd(), "IRCurve requires AmountUSD for FX conversion")
        self.assertEqual(rec.type(), ORE.CrifRecord.RecordType_SIMM)

    def test_crifrecord_is_simm_parameter(self) -> None:
        """isSimmParameter() returns True for ProductClassMultiplier and False for IRCurve."""
        nsd = ORE.NettingSetDetails("NS1")
        ir_rec = ORE.CrifRecord(
            "t1", "Swap", nsd,
            ORE.CrifRecord.ProductClass_RatesFX,
            ORE.CrifRecord.RiskType_IRCurve,
            "USD", "1", "2y", "Libor3m", "USD", -500000.0, -500000.0,
        )
        param_rec = ORE.CrifRecord(
            "", "", nsd,
            ORE.CrifRecord.ProductClass_Empty,
            ORE.CrifRecord.RiskType_ProductClassMultiplier,
            "RatesFX", "", "", "", "", 1.0, 0.0,
        )
        self.assertFalse(ir_rec.isSimmParameter())
        self.assertTrue(param_rec.isSimmParameter())

    def test_crifrecord_field_mutation(self) -> None:
        """Data members are writable from Python."""
        rec = ORE.CrifRecord()
        rec.tradeId = "my_trade"
        rec.qualifier = "EUR"
        rec.amount = 999.0
        self.assertEqual(rec.tradeId, "my_trade")
        self.assertEqual(rec.qualifier, "EUR")
        self.assertAlmostEqual(rec.amount, 999.0)

    def test_crif_records_method_and_iteration(self) -> None:
        """Crif.records() and __iter__ work on a programmatically-built Crif.

        Uses addRecord() to populate the Crif without going through the CSV
        loader, confirming that the iteration path works independently of CSV
        parsing.
        """
        nsd = ORE.NettingSetDetails("NS1")
        rec = ORE.CrifRecord(
            "t1", "Swap", nsd,
            ORE.CrifRecord.ProductClass_RatesFX,
            ORE.CrifRecord.RiskType_IRCurve,
            "USD", "1", "2y", "Libor3m", "USD", -500000.0, -500000.0,
        )
        crif = ORE.Crif()
        crif.addRecord(rec)
        self.assertEqual(crif.size(), 1)

        # records() returns a CrifRecordVector with one element
        recs = crif.records()
        self.assertEqual(len(recs), 1)
        retrieved = recs[0]
        self.assertEqual(retrieved.tradeId, "t1")
        self.assertEqual(retrieved.riskType, ORE.CrifRecord.RiskType_IRCurve)
        self.assertAlmostEqual(retrieved.amountUsd, -500000.0)

        # __iter__ enables for-loop syntax; verify key fields from each record
        count = 0
        for record in crif:
            self.assertEqual(record.tradeId, "t1")
            self.assertEqual(record.qualifier, "USD")
            self.assertAlmostEqual(record.amount, -500000.0)
            count += 1
        self.assertEqual(count, 1)

    def test_crif_len(self) -> None:
        """len(crif) equals crif.size() for both empty and non-empty Crif."""
        crif = ORE.Crif()
        self.assertEqual(len(crif), 0)
        self.assertEqual(len(crif), crif.size())

        nsd = ORE.NettingSetDetails("NS1")
        rec = ORE.CrifRecord(
            "t1", "Swap", nsd,
            ORE.CrifRecord.ProductClass_RatesFX,
            ORE.CrifRecord.RiskType_IRCurve,
            "USD", "1", "2y", "Libor3m", "USD", -500000.0, -500000.0,
        )
        crif.addRecord(rec)
        self.assertEqual(len(crif), 1)
        self.assertEqual(len(crif), crif.size())


if __name__ == "__main__":
    unittest.main()

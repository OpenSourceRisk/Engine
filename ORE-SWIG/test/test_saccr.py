"""Tests for SA-CCR SWIG bindings.

Covers:
  - Symbol availability for SaccrTradeData, SaccrCalculator, and related types
  - SaccrTradeData enum values (AssetClass, CommodityHedgingSet)
  - SaccrContribution struct construction and plain-field access
  - optional<Real> fields map to Python None when not set
  - optional<Size> field maps to Python None when not set
  - %extend helpers on SaccrContribution (hedgingSet, qualifier, saccrAssetClassInt, etc.)
  - SaccrAnalytic exposes saccrCalculator() and saccrTradeData() accessors
  - NettingSetDetailsSet and SaccrImplMap container templates are usable
  - CrifRecord standard and SA-CCR field accessors
  - CrifRecord saccrLabel1/saccrLabel2 variant helpers
  - InputParameters.setCounterpartyManager() symbol availability
"""

import unittest

import ORE


class SaccrSymbolTest(unittest.TestCase):
    """Assert all required SA-CCR symbols are exported by the ORE module."""

    def test_saccr_calculator_symbol(self) -> None:
        self.assertTrue(hasattr(ORE, "SaccrCalculator"), msg="Missing ORE.SaccrCalculator")

    def test_saccr_trade_data_symbol(self) -> None:
        self.assertTrue(hasattr(ORE, "SaccrTradeData"), msg="Missing ORE.SaccrTradeData")

    def test_contribution_symbol(self) -> None:
        """SaccrContribution is exposed both at module level and as SaccrTradeData.Contribution."""
        self.assertTrue(hasattr(ORE, "SaccrContribution"),
                        msg="Missing ORE.SaccrContribution")
        self.assertTrue(hasattr(ORE.SaccrTradeData, "Contribution"),
                        msg="Missing ORE.SaccrTradeData.Contribution (injected by %pythoncode)")

    def test_impl_symbol(self) -> None:
        """SaccrImpl is exposed both at module level and as SaccrTradeData.Impl."""
        self.assertTrue(hasattr(ORE, "SaccrImpl"),
                        msg="Missing ORE.SaccrImpl")
        self.assertTrue(hasattr(ORE.SaccrTradeData, "Impl"),
                        msg="Missing ORE.SaccrTradeData.Impl (injected by %pythoncode)")

    def test_container_templates_available(self) -> None:
        """STL container templates are importable from ORE."""
        for name in ("NettingSetDetailsSet", "SaccrContributionVector",
                     "SaccrAssetClassSet", "SaccrImplMap"):
            self.assertTrue(hasattr(ORE, name), msg=f"Missing ORE.{name}")

    def test_saccr_analytic_accessors_exist(self) -> None:
        """SaCcrAnalytic (renamed SaccrAnalytic) exposes the two new accessors."""
        analytic = ORE.SaccrAnalytic()
        self.assertTrue(
            hasattr(analytic, "saccrCalculator"),
            msg="SaccrAnalytic missing saccrCalculator()",
        )
        self.assertTrue(
            hasattr(analytic, "saccrTradeData"),
            msg="SaccrAnalytic missing saccrTradeData()",
        )


class SaccrAssetClassEnumTest(unittest.TestCase):
    """Validate AssetClass and CommodityHedgingSet enum values."""

    def test_asset_class_values_present(self) -> None:
        td = ORE.SaccrTradeData
        # SWIG renames the C++ keyword 'None' to '_None'
        for name in ("IR", "FX", "Credit", "Equity", "Commodity", "_None"):
            self.assertTrue(
                hasattr(td, name),
                msg=f"SaccrTradeData.AssetClass missing: {name}",
            )

    def test_asset_class_values_distinct(self) -> None:
        td = ORE.SaccrTradeData
        # Use _None — SWIG renames C++ 'None' enum value to '_None'
        values = [td.IR, td.FX, td.Credit, td.Equity, td.Commodity, td._None]
        self.assertEqual(len(set(values)), len(values))

    def test_commodity_hedging_set_values_present(self) -> None:
        """CommodityHedgingSet (enum class) values are prefixed with the enum name."""
        td = ORE.SaccrTradeData
        for name in ("CommodityHedgingSet_Energy", "CommodityHedgingSet_Agriculture",
                     "CommodityHedgingSet_Metal", "CommodityHedgingSet_Other"):
            self.assertTrue(
                hasattr(td, name),
                msg=f"SaccrTradeData missing: {name}",
            )


class SaccrContributionTest(unittest.TestCase):
    """SaccrContribution struct construction and field access.

    SaccrContribution is also accessible as ORE.SaccrTradeData.Contribution
    via the %pythoncode injection in orea_saccr.i.
    """

    def _make_contribution(self) -> "ORE.SaccrContribution":
        """Return a default-constructed SaccrContribution."""
        return ORE.SaccrContribution()

    def test_default_construct_via_module(self) -> None:
        c = ORE.SaccrContribution()
        self.assertIsNotNone(c)

    def test_default_construct_via_outer_class(self) -> None:
        """SaccrTradeData.Contribution is an alias for SaccrContribution."""
        c = ORE.SaccrTradeData.Contribution()
        self.assertIsNotNone(c)
        self.assertIsInstance(c, ORE.SaccrContribution)

    def test_plain_string_field_currency(self) -> None:
        c = self._make_contribution()
        self.assertIsInstance(c.currency, str)

    def test_plain_string_field_bucket(self) -> None:
        c = self._make_contribution()
        self.assertIsInstance(c.bucket, str)

    def test_plain_bool_fields(self) -> None:
        c = self._make_contribution()
        self.assertIsInstance(c.isOption, bool)
        self.assertIsInstance(c.isVol, bool)

    def test_optional_real_fields_are_none_by_default(self) -> None:
        """optional<Real> fields that are unset must map to Python None."""
        c = self._make_contribution()
        for field in (
            "supervisoryDuration",
            "startDate",
            "endDate",
            "lastExerciseDate",
            "currentPrice",
            "optionDeltaPrice",
            "strike",
        ):
            val = getattr(c, field)
            self.assertIsNone(
                val,
                msg=f"SaccrContribution.{field} should be None for default-constructed struct, got {val!r}",
            )

    def test_optional_size_field_is_none_by_default(self) -> None:
        """optional<Size> field numNominalFlows maps to Python None when unset."""
        c = self._make_contribution()
        self.assertIsNone(
            c.numNominalFlows,
            msg="numNominalFlows should be None for default-constructed struct",
        )

    def test_extend_helpers_available(self) -> None:
        """The %extend helper methods exist on SaccrContribution."""
        c = self._make_contribution()
        for method in ("hedgingSet", "isBasis", "isVolHedging", "qualifier",
                       "saccrAssetClassInt", "isIndex"):
            self.assertTrue(
                hasattr(c, method),
                msg=f"SaccrContribution missing %extend helper: {method}",
            )

    def test_hedging_set_returns_string(self) -> None:
        c = self._make_contribution()
        self.assertIsInstance(c.hedgingSet(), str)

    def test_qualifier_returns_string(self) -> None:
        c = self._make_contribution()
        self.assertIsInstance(c.qualifier(), str)

    def test_saccr_asset_class_int_returns_int(self) -> None:
        c = self._make_contribution()
        result = c.saccrAssetClassInt()
        self.assertIsInstance(result, int)

    def test_is_index_returns_bool(self) -> None:
        c = self._make_contribution()
        self.assertIsInstance(c.isIndex(), bool)

    def test_is_basis_returns_bool(self) -> None:
        c = self._make_contribution()
        self.assertIsInstance(c.isBasis(), bool)

    def test_is_vol_hedging_returns_bool(self) -> None:
        c = self._make_contribution()
        self.assertIsInstance(c.isVolHedging(), bool)


class SaccrContributionVectorTest(unittest.TestCase):
    """SaccrContributionVector template wraps std::vector<SaccrContribution>."""

    def test_create_empty_vector(self) -> None:
        vec = ORE.SaccrContributionVector()
        self.assertEqual(len(vec), 0)

    def test_push_and_index(self) -> None:
        vec = ORE.SaccrContributionVector()
        c = ORE.SaccrContribution()
        vec.push_back(c)
        self.assertEqual(len(vec), 1)
        retrieved = vec[0]
        self.assertIsNotNone(retrieved)


class NettingSetDetailsSetTest(unittest.TestCase):
    """NettingSetDetailsSet wraps std::set<NettingSetDetails>."""

    def test_create_empty_set(self) -> None:
        s = ORE.NettingSetDetailsSet()
        self.assertEqual(len(s), 0)


class SaccrAnalyticConstructTest(unittest.TestCase):
    """SaCcrAnalytic (exposed as SaccrAnalytic) constructs and exposes accessors."""

    def test_default_construct(self) -> None:
        analytic = ORE.SaccrAnalytic()
        self.assertIsNotNone(analytic)

    def test_saccrCalculator_callable_before_run(self) -> None:
        """saccrCalculator() is callable before the analytic has run.

        The QuantLib-SWIG %shared_ptr typemap wraps null shared_ptrs as Python
        None — this tests the method is accessible and
        doesn't raise on a fresh analytic instance.
        """
        analytic = ORE.SaccrAnalytic()
        # Calling the accessor must not raise
        result = analytic.saccrCalculator()
        self.assertIsNone(result)

    def test_saccrTradeData_callable_before_run(self) -> None:
        """saccrTradeData() is callable before the analytic has run."""
        analytic = ORE.SaccrAnalytic()
        result = analytic.saccrTradeData()
        self.assertIsNone(result)


if __name__ == "__main__":
    unittest.main()


class CrifRecordFieldTest(unittest.TestCase):
    """CrifRecord standard and SA-CCR field accessors exposed via SWIG."""

    def _make_record(self) -> "ORE.CrifRecord":
        return ORE.CrifRecord()

    def test_default_construct(self) -> None:
        r = self._make_record()
        self.assertIsNotNone(r)

    def test_string_fields_readable_and_writable(self) -> None:
        r = self._make_record()
        for field, value in (
            ("tradeId", "TRD-001"),
            ("tradeType", "IRS"),
            ("qualifier", "USD"),
            ("bucket", "3"),
            ("label1", "2Y"),
            ("label2", "OIS"),
            ("amountCurrency", "USD"),
        ):
            setattr(r, field, value)
            self.assertEqual(getattr(r, field), value,
                             msg=f"CrifRecord.{field} round-trip failed")

    def test_real_fields_readable_and_writable(self) -> None:
        r = self._make_record()
        r.amount = 1_000_000.0
        self.assertAlmostEqual(r.amount, 1_000_000.0)
        r.amountUsd = 950_000.0
        self.assertAlmostEqual(r.amountUsd, 950_000.0)
        r.saccrEndDate = 12345.0
        self.assertAlmostEqual(r.saccrEndDate, 12345.0)

    def test_risktype_field(self) -> None:
        r = self._make_record()
        r.riskType = ORE.CrifRecord.RiskType_IRCurve
        self.assertEqual(r.riskType, ORE.CrifRecord.RiskType_IRCurve)

    def test_regulation_field(self) -> None:
        r = self._make_record()
        r.regulation = ORE.CrifRecord.SaccrRegulation_Basel
        self.assertEqual(r.regulation, ORE.CrifRecord.SaccrRegulation_Basel)

    def test_netting_set_details_field(self) -> None:
        r = self._make_record()
        nsd = ORE.NettingSetDetails("NS-001")
        r.nettingSetDetails = nsd
        self.assertEqual(r.nettingSetDetails.nettingSetId(), "NS-001")


class CrifRecordVariantHelperTest(unittest.TestCase):
    """saccrLabel1/saccrLabel2 variant %extend helpers on CrifRecord."""

    def _make_record(self) -> "ORE.CrifRecord":
        return ORE.CrifRecord()

    def test_saccrLabel1_default_is_string_type(self) -> None:
        """Default saccrLabel1 = '' → variant holds string (which() == 1)."""
        r = self._make_record()
        self.assertEqual(r.saccrLabel1Type(), 1,
                         msg="Default saccrLabel1 should hold string (type index 1)")

    def test_saccrLabel1_default_string_value(self) -> None:
        r = self._make_record()
        self.assertEqual(r.saccrLabel1AsString(), "")

    def test_saccrLabel2_default_is_string_type(self) -> None:
        """Default saccrLabel2 = '' → variant holds string (which() == 1)."""
        r = self._make_record()
        self.assertEqual(r.saccrLabel2Type(), 1,
                         msg="Default saccrLabel2 should hold string (type index 1)")

    def test_saccrLabel2_default_string_value(self) -> None:
        r = self._make_record()
        self.assertEqual(r.saccrLabel2AsString(), "")

    def test_variant_helper_methods_exist(self) -> None:
        r = self._make_record()
        for method in (
            "saccrLabel1Type", "saccrLabel1AsReal", "saccrLabel1AsString", "saccrLabel1AsSize",
            "saccrLabel2Type", "saccrLabel2AsReal", "saccrLabel2AsString",
        ):
            self.assertTrue(hasattr(r, method),
                            msg=f"CrifRecord missing %extend helper: {method}")


class InputParametersCounterpartyManagerTest(unittest.TestCase):
    """InputParameters.setCounterpartyManager() is exposed and callable."""

    def test_set_counterparty_manager_symbol_exists(self) -> None:
        ip = ORE.InputParameters()
        self.assertTrue(
            hasattr(ip, "setCounterpartyManager"),
            msg="InputParameters missing setCounterpartyManager()",
        )

    def test_set_counterparty_manager_shared_ptr_overload(self) -> None:
        ip = ORE.InputParameters()
        cm = ORE.CounterpartyManager()
        # Should not raise
        ip.setCounterpartyManager(cm)

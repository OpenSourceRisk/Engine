"""Tests for MarketRiskConfiguration enums and RiskFilter bindings.

Validates SWIG wrapping of MarketRiskConfiguration::RiskClass,
MarketRiskConfiguration::RiskType, the riskClasses()/riskTypes() helpers,
parseVarRiskClass()/parseVarRiskType() free functions, and RiskFilter.
"""

import unittest
import ORE


class TestMarketRiskConfigurationEnums(unittest.TestCase):
    """Enum values are accessible and have correct integer ordering."""

    def test_risk_class_values_exist(self):
        cls = ORE.MarketRiskConfiguration
        self.assertIsNotNone(cls.RiskClass_All)
        self.assertIsNotNone(cls.RiskClass_InterestRate)
        self.assertIsNotNone(cls.RiskClass_Inflation)
        self.assertIsNotNone(cls.RiskClass_Credit)
        self.assertIsNotNone(cls.RiskClass_Equity)
        self.assertIsNotNone(cls.RiskClass_FX)
        self.assertIsNotNone(cls.RiskClass_Commodity)

    def test_risk_type_values_exist(self):
        cls = ORE.MarketRiskConfiguration
        self.assertIsNotNone(cls.RiskType_All)
        self.assertIsNotNone(cls.RiskType_DeltaGamma)
        self.assertIsNotNone(cls.RiskType_Vega)
        self.assertIsNotNone(cls.RiskType_BaseCorrelation)

    def test_risk_class_roundtrip_parse(self):
        rc = ORE.parseVarRiskClass("InterestRate")
        # Enum value should equal itself
        self.assertEqual(rc, ORE.MarketRiskConfiguration.RiskClass_InterestRate)

    def test_risk_type_roundtrip_parse(self):
        rt = ORE.parseVarRiskType("DeltaGamma")
        self.assertEqual(rt, ORE.MarketRiskConfiguration.RiskType_DeltaGamma)

    def test_parse_all_classes(self):
        for name in ("All", "InterestRate", "Inflation", "Credit", "Equity", "FX", "Commodity"):
            rc = ORE.parseVarRiskClass(name)
            self.assertIsNotNone(rc)

    def test_parse_all_types(self):
        for name in ("All", "DeltaGamma", "Vega", "BaseCorrelation"):
            rt = ORE.parseVarRiskType(name)
            self.assertIsNotNone(rt)


class TestRiskClassesRiskTypes(unittest.TestCase):
    """Static helpers riskClasses() and riskTypes() return correct collections."""

    def test_risk_classes_excludes_all_by_default(self):
        classes = ORE.MarketRiskConfiguration.riskClasses()
        values = list(classes)
        # Should return 6 values (not including All)
        self.assertEqual(len(values), 6)

    def test_risk_classes_includes_all_when_requested(self):
        classes = ORE.MarketRiskConfiguration.riskClasses(True)
        values = list(classes)
        # Should return 7 values (including All)
        self.assertEqual(len(values), 7)

    def test_risk_types_excludes_all_by_default(self):
        types = ORE.MarketRiskConfiguration.riskTypes()
        values = list(types)
        # Should return 3 values (not including All)
        self.assertEqual(len(values), 3)

    def test_risk_types_includes_all_when_requested(self):
        types = ORE.MarketRiskConfiguration.riskTypes(True)
        values = list(types)
        # Should return 4 values (including All)
        self.assertEqual(len(values), 4)


class TestRiskFilter(unittest.TestCase):
    """RiskFilter wraps ScenarioFilter and filters RiskFactorKeys by class/type."""

    def _ir_key(self):
        return ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "USD", 0)

    def _fx_key(self):
        return ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_FXSpot, "EURUSD", 0)

    def _eq_key(self):
        return ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_EquitySpot, "SPX", 0)

    def test_constructible(self):
        rf = ORE.RiskFilter(
            ORE.MarketRiskConfiguration.RiskClass_FX,
            ORE.MarketRiskConfiguration.RiskType_All,
        )
        self.assertIsNotNone(rf)

    def test_fx_filter_allows_fx_key(self):
        rf = ORE.RiskFilter(
            ORE.MarketRiskConfiguration.RiskClass_FX,
            ORE.MarketRiskConfiguration.RiskType_All,
        )
        self.assertTrue(rf.allow(self._fx_key()))

    def test_fx_filter_rejects_ir_key(self):
        rf = ORE.RiskFilter(
            ORE.MarketRiskConfiguration.RiskClass_FX,
            ORE.MarketRiskConfiguration.RiskType_All,
        )
        self.assertFalse(rf.allow(self._ir_key()))

    def test_ir_filter_allows_ir_key(self):
        rf = ORE.RiskFilter(
            ORE.MarketRiskConfiguration.RiskClass_InterestRate,
            ORE.MarketRiskConfiguration.RiskType_All,
        )
        self.assertTrue(rf.allow(self._ir_key()))

    def test_ir_filter_rejects_fx_key(self):
        rf = ORE.RiskFilter(
            ORE.MarketRiskConfiguration.RiskClass_InterestRate,
            ORE.MarketRiskConfiguration.RiskType_All,
        )
        self.assertFalse(rf.allow(self._fx_key()))

    def test_all_filter_allows_any_key(self):
        rf = ORE.RiskFilter(
            ORE.MarketRiskConfiguration.RiskClass_All,
            ORE.MarketRiskConfiguration.RiskType_All,
        )
        self.assertTrue(rf.allow(self._ir_key()))
        self.assertTrue(rf.allow(self._fx_key()))
        self.assertTrue(rf.allow(self._eq_key()))

    def test_is_scenario_filter_subclass(self):
        """RiskFilter is usable as a ScenarioFilter."""
        rf = ORE.RiskFilter(
            ORE.MarketRiskConfiguration.RiskClass_Equity,
            ORE.MarketRiskConfiguration.RiskType_DeltaGamma,
        )
        self.assertIsInstance(rf, ORE.ScenarioFilter)

    def test_several_risk_classes_constructible(self):
        for rc_enum in [ORE.MarketRiskConfiguration.RiskClass_FX,
                        ORE.MarketRiskConfiguration.RiskClass_Equity,
                        ORE.MarketRiskConfiguration.RiskClass_Commodity]:
            rf = ORE.RiskFilter(rc_enum, ORE.MarketRiskConfiguration.RiskType_All)
            self.assertIsNotNone(rf)

    def test_several_risk_types_constructible(self):
        for rt_enum in [ORE.MarketRiskConfiguration.RiskType_DeltaGamma,
                        ORE.MarketRiskConfiguration.RiskType_Vega]:
            rf = ORE.RiskFilter(ORE.MarketRiskConfiguration.RiskClass_All, rt_enum)
            self.assertIsNotNone(rf)

    def test_multiple_filters_independent(self):
        """Different filter instances filter independently."""
        fx_filter = ORE.RiskFilter(
            ORE.MarketRiskConfiguration.RiskClass_FX,
            ORE.MarketRiskConfiguration.RiskType_All,
        )
        ir_filter = ORE.RiskFilter(
            ORE.MarketRiskConfiguration.RiskClass_InterestRate,
            ORE.MarketRiskConfiguration.RiskType_All,
        )
        fx_key = self._fx_key()
        ir_key = self._ir_key()
        self.assertTrue(fx_filter.allow(fx_key))
        self.assertFalse(fx_filter.allow(ir_key))
        self.assertFalse(ir_filter.allow(fx_key))
        self.assertTrue(ir_filter.allow(ir_key))


if __name__ == "__main__":
    unittest.main()



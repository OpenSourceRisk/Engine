"""
 Copyright (C) 2026 Quaternion Risk Management Ltd
 All rights reserved.

 Tests for Phase 2 OREData SWIG bindings:
   FXTriangulation, WrappedMarket, TodaysMarketCalibrationInfo structs,
   and the applyFixings free function.
"""

import unittest
from ORE import *


class FXTriangulationTest(unittest.TestCase):

    def test_default_construction(self):
        """Empty FXTriangulation can be constructed."""
        tri = FXTriangulation()
        self.assertIsNotNone(tri)

    def test_construction_from_parallel_vectors(self):
        """FXTriangulation can be populated from aligned pair/quote vectors."""
        spot = SimpleQuote(1.25)
        handle = QuoteHandle(spot)
        pairs = StrVector(["EURUSD"])
        quotes = QuoteHandleVector([handle])
        tri = FXTriangulation(pairs, quotes)
        self.assertIsNotNone(tri)

    def test_get_quote_returns_handle(self):
        """getQuote() returns a live Handle<Quote> matching the provided value."""
        spot = SimpleQuote(1.25)
        handle = QuoteHandle(spot)
        pairs = StrVector(["EURUSD"])
        quotes = QuoteHandleVector([handle])
        tri = FXTriangulation(pairs, quotes)
        q = tri.getQuote("EURUSD")
        self.assertAlmostEqual(q.value(), 1.25)

    def test_get_quote_inverse_pair_triangulates(self):
        """getQuote() triangulates the inverse pair correctly."""
        spot = SimpleQuote(1.25)
        pairs = StrVector(["EURUSD"])
        quotes = QuoteHandleVector([QuoteHandle(spot)])
        tri = FXTriangulation(pairs, quotes)
        q = tri.getQuote("USDEUR")
        self.assertAlmostEqual(q.value(), 1.0 / 1.25, places=10)

    def test_missing_pair_raises(self):
        """getQuote() raises for an unknown pair."""
        tri = FXTriangulation()
        with self.assertRaises(Exception):
            tri.getQuote("GBPUSD")


class CalibrationInfoStructsTest(unittest.TestCase):

    def test_yield_curve_calibration_info_accessible(self):
        """YieldCurveCalibrationInfo is a constructible struct with expected fields."""
        info = YieldCurveCalibrationInfo()
        self.assertIsNotNone(info)
        self.assertIsInstance(info.dayCounter, str)
        self.assertIsInstance(info.currency, str)
        # vector fields are empty by default
        self.assertEqual(len(info.pillarDates), 0)
        self.assertEqual(len(info.zeroRates), 0)
        self.assertEqual(len(info.discountFactors), 0)

    def test_fitted_bond_curve_calibration_info(self):
        """FittedBondCurveCalibrationInfo inherits from YieldCurveCalibrationInfo."""
        info = FittedBondCurveCalibrationInfo()
        self.assertIsNotNone(info)
        self.assertIsInstance(info.fittingMethod, str)
        # Inherited fields
        self.assertEqual(len(info.pillarDates), 0)

    def test_inflation_curve_calibration_info(self):
        """InflationCurveCalibrationInfo is a constructible struct."""
        info = InflationCurveCalibrationInfo()
        self.assertIsNotNone(info)
        self.assertIsInstance(info.dayCounter, str)
        self.assertEqual(len(info.pillarDates), 0)

    def test_commodity_curve_calibration_info(self):
        """CommodityCurveCalibrationInfo is a constructible struct."""
        info = CommodityCurveCalibrationInfo()
        self.assertIsNotNone(info)
        self.assertIsInstance(info.currency, str)

    def test_fx_eq_comm_vol_calibration_info(self):
        """FxEqCommVolCalibrationInfo is constructible and has expected fields."""
        info = FxEqCommVolCalibrationInfo()
        self.assertIsNotNone(info)
        self.assertIsInstance(info.isArbitrageFree, bool)
        self.assertEqual(len(info.expiryDates), 0)

    def test_ir_vol_calibration_info(self):
        """IrVolCalibrationInfo has nested triple-vector fields accessible."""
        info = IrVolCalibrationInfo()
        self.assertIsNotNone(info)
        self.assertIsInstance(info.isArbitrageFree, bool)
        self.assertEqual(len(info.underlyingTenors), 0)

    def test_cpi_vol_calibration_info(self):
        """CpiVolCalibrationInfo is constructible."""
        info = CpiVolCalibrationInfo()
        self.assertIsNotNone(info)
        self.assertEqual(len(info.expiryDates), 0)

    def test_todays_market_calibration_info(self):
        """TodaysMarketCalibrationInfo container is constructible."""
        info = TodaysMarketCalibrationInfo()
        self.assertIsNotNone(info)
        self.assertEqual(len(info.yieldCurveCalibrationInfo), 0)
        self.assertEqual(len(info.irVolCalibrationInfo), 0)


class ApplyFixingsTest(unittest.TestCase):

    def test_apply_empty_fixings_does_not_raise(self):
        """applyFixings with an empty set completes without error."""
        empty = FixingSet()
        applyFixings(empty)

    def test_apply_fixings_with_one_entry(self):
        """applyFixings accepts a FixingSet with a single Fixing."""
        Settings.instance().evaluationDate = Date(5, February, 2016)
        f = Fixing(Date(4, February, 2016), "USD-LIBOR-3M", 0.003)
        fs = FixingSet([f])
        # Should not raise; fixing is applied to the QuantLib index manager
        applyFixings(fs)


class WrappedMarketSymbolsTest(unittest.TestCase):

    def test_wrapped_market_class_exposed(self):
        """WrappedMarket class is available in the ORE module."""
        import ORE
        self.assertTrue(hasattr(ORE, "WrappedMarket"),
                        "WrappedMarket missing from ORE module")

    def test_phase2_symbols_in_module(self):
        """All Phase 2 symbol names are present in the ORE module."""
        import ORE
        symbols = [
            "FXTriangulation",
            "WrappedMarket",
            "YieldCurveCalibrationInfo",
            "FittedBondCurveCalibrationInfo",
            "InflationCurveCalibrationInfo",
            "ZeroInflationCurveCalibrationInfo",
            "YoYInflationCurveCalibrationInfo",
            "CommodityCurveCalibrationInfo",
            "FxEqCommVolCalibrationInfo",
            "IrVolCalibrationInfo",
            "CpiVolCalibrationInfo",
            "TodaysMarketCalibrationInfo",
            "applyFixings",
        ]
        for sym in symbols:
            self.assertTrue(hasattr(ORE, sym),
                            f"Phase 2 symbol missing from ORE module: {sym}")


if __name__ == "__main__":
    import ORE as _ORE
    print("testing ORE " + _ORE.__version__)
    unittest.main(verbosity=2)

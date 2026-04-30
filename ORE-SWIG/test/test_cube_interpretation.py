"""Tests for CubeInterpretation and DateGrid Python bindings."""

import unittest

import ORE


class DateGridBindingTest(unittest.TestCase):
    """Validate DateGrid SWIG bindings."""

    def test_symbol_exists(self) -> None:
        """Assert DateGrid is exposed in the ORE module."""
        self.assertTrue(hasattr(ORE, "DateGrid"))

    def test_default_constructor(self) -> None:
        """Construct a default DateGrid (single date at evaluation date)."""
        dg = ORE.DateGrid()
        self.assertIsNotNone(dg)
        self.assertGreaterEqual(dg.size(), 1)

    def test_string_constructor(self) -> None:
        """Construct DateGrid from a grid specification string."""
        dg = ORE.DateGrid("3M,1Y")
        self.assertGreater(dg.size(), 0)
        self.assertGreater(len(dg.dates()), 0)
        self.assertGreater(len(dg.tenors()), 0)

    def test_dates_constructor(self) -> None:
        """Construct DateGrid from an explicit list of dates."""
        today = ORE.Settings.instance().evaluationDate
        dates = ORE.DateVector()
        dates.append(today + ORE.Period(3, ORE.Months))
        dates.append(today + ORE.Period(6, ORE.Months))
        dates.append(today + ORE.Period(1, ORE.Years))
        dg = ORE.DateGrid(dates)
        self.assertEqual(dg.size(), 3)

    def test_inspectors(self) -> None:
        """Verify key inspectors return expected types."""
        dg = ORE.DateGrid("1M,3M,6M,1Y")
        self.assertIsNotNone(dg.calendar())
        self.assertIsNotNone(dg.dayCounter())
        self.assertGreater(len(dg.times()), 0)
        self.assertGreater(len(dg.dates()), 0)

    def test_close_out_dates(self) -> None:
        """Verify addCloseOutDates and closeOutDates work."""
        dg = ORE.DateGrid("1M,3M,6M,1Y")
        dg.addCloseOutDates(ORE.Period(2, ORE.Weeks))
        co_dates = dg.closeOutDates()
        self.assertGreater(len(co_dates), 0)

    def test_valuation_and_close_out_flags(self) -> None:
        """Verify isValuationDate and isCloseOutDate return bool vectors."""
        dg = ORE.DateGrid("1M,3M,6M,1Y")
        dg.addCloseOutDates(ORE.Period(2, ORE.Weeks))
        val_flags = dg.isValuationDate()
        co_flags = dg.isCloseOutDate()
        self.assertEqual(len(val_flags), dg.size())
        self.assertEqual(len(co_flags), dg.size())


class CubeInterpretationBindingTest(unittest.TestCase):
    """Validate CubeInterpretation SWIG bindings."""

    def test_symbol_exists(self) -> None:
        """Assert CubeInterpretation is exposed in the ORE module."""
        self.assertTrue(hasattr(ORE, "CubeInterpretation"))

    def test_minimal_constructor(self) -> None:
        """Construct CubeInterpretation with minimal arguments."""
        ci = ORE.CubeInterpretation(True, False)
        self.assertIsNotNone(ci)

    def test_full_constructor_no_close_out(self) -> None:
        """Construct with all arguments, no close-out lag."""
        ci = ORE.CubeInterpretation(True, False, False, None, 0, False)
        self.assertIsNotNone(ci)
        self.assertTrue(ci.storeFlows())
        self.assertFalse(ci.withCloseOutLag())
        self.assertFalse(ci.withExerciseValue())
        self.assertEqual(ci.storeCreditStateNPVs(), 0)
        self.assertFalse(ci.flipViewXVA())

    def test_constructor_with_date_grid(self) -> None:
        """Construct with a DateGrid for close-out lag interpretation."""
        dg = ORE.DateGrid("1M,3M,6M,1Y")
        dg.addCloseOutDates(ORE.Period(2, ORE.Weeks))
        ci = ORE.CubeInterpretation(True, True, False, dg, 0, False)
        self.assertIsNotNone(ci)
        self.assertTrue(ci.withCloseOutLag())
        # Round-trip: dateGrid() should return a non-null DateGrid
        returned_dg = ci.dateGrid()
        self.assertIsNotNone(returned_dg)
        self.assertEqual(returned_dg.size(), dg.size())

    def test_required_npv_cube_depth(self) -> None:
        """Verify requiredNpvCubeDepth returns a positive integer."""
        ci = ORE.CubeInterpretation(True, False)
        depth = ci.requiredNpvCubeDepth()
        self.assertGreaterEqual(depth, 1)

    def test_default_date_npv_index(self) -> None:
        """defaultDateNpvIndex should be 0 for standard interpretation."""
        ci = ORE.CubeInterpretation(False, False)
        idx = ci.defaultDateNpvIndex()
        self.assertEqual(idx, 0)

    def test_close_out_date_npv_index_with_lag(self) -> None:
        """closeOutDateNpvIndex should be valid when withCloseOutLag."""
        dg = ORE.DateGrid("1M,3M,6M,1Y")
        dg.addCloseOutDates(ORE.Period(2, ORE.Weeks))
        ci = ORE.CubeInterpretation(False, True, False, dg)
        idx = ci.closeOutDateNpvIndex()
        # Should be a valid non-sentinel index
        self.assertIsInstance(idx, int)
        self.assertGreater(idx, 0)

    def test_exercise_value_index(self) -> None:
        """exerciseValueIndex should be valid when withExerciseValue."""
        ci = ORE.CubeInterpretation(False, False, True)
        idx = ci.exerciseValueIndex()
        self.assertIsInstance(idx, int)
        self.assertGreater(idx, 0)

    def test_mpor_flows_index(self) -> None:
        """mporFlowsIndex should be valid when storeFlows is True."""
        ci = ORE.CubeInterpretation(True, False)
        idx = ci.mporFlowsIndex()
        self.assertIsInstance(idx, int)
        self.assertGreater(idx, 0)

    def test_close_out_lag_without_dategrid_raises(self) -> None:
        """Constructing with withCloseOutLag=True but no DateGrid raises."""
        with self.assertRaises(RuntimeError):
            ORE.CubeInterpretation(False, True, False, None)

    def test_depth_increases_with_features(self) -> None:
        """More features enabled should require greater cube depth."""
        ci_min = ORE.CubeInterpretation(False, False, False)
        dg = ORE.DateGrid("1M,3M")
        dg.addCloseOutDates(ORE.Period(2, ORE.Weeks))
        ci_max = ORE.CubeInterpretation(True, True, True, dg, 3, False)
        self.assertGreater(
            ci_max.requiredNpvCubeDepth(),
            ci_min.requiredNpvCubeDepth(),
        )


if __name__ == "__main__":
    unittest.main()

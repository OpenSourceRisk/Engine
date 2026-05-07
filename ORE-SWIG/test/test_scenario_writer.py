"""Tests for ScenarioWriter SWIG bindings.

Covers: ScenarioWriter (file-based ctors), round-trip with ScenarioFileReader.
"""

import os
import tempfile
import unittest

import ORE


def _make_factory():
    return ORE.SimpleScenarioFactory()


def _make_keyed_scenario(factory, date, discount_val=0.99500, fx_val=1.12000):
    """Build a SimpleScenario with two standard risk factor keys."""
    s = factory.buildScenario(date, True)
    s.add(ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "EUR", 0),
          discount_val)
    s.add(ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_FXSpot, "EURUSD", 0),
          fx_val)
    return s


class TestSymbolsAvailable(unittest.TestCase):

    def test_scenario_writer_present(self):
        self.assertTrue(hasattr(ORE, "ScenarioWriter"),
                        msg="Missing ORE symbol: ScenarioWriter")
        self.assertTrue(hasattr(ORE, "RiskFactorKeyVector"),
                        msg="Missing ORE symbol: RiskFactorKeyVector")


class TestScenarioWriterConstruction(unittest.TestCase):

    def test_file_only_constructor(self):
        with tempfile.NamedTemporaryFile(suffix=".csv", delete=False) as f:
            fname = f.name
        try:
            w = ORE.ScenarioWriter(fname)
            self.assertIsNotNone(w)
            w.close()
        finally:
            del w
            try:
                os.unlink(fname)
            except PermissionError:
                pass

    def test_src_file_constructor(self):
        """ScenarioWriter(generator, filename) two-arg constructor works."""
        src_gen = ORE.StaticScenarioGenerator()
        with tempfile.NamedTemporaryFile(suffix=".csv", delete=False) as f:
            fname = f.name
        writer = None
        try:
            writer = ORE.ScenarioWriter(src_gen, fname)
            self.assertIsNotNone(writer)
            writer.close()
        finally:
            del writer
            try:
                os.unlink(fname)
            except PermissionError:
                pass


class TestScenarioWriterRoundTrip(unittest.TestCase):
    """Write scenarios with ScenarioWriter, read back with ScenarioFileReader."""

    def setUp(self):
        self.factory = _make_factory()
        self.tmp = tempfile.NamedTemporaryFile(
            suffix=".csv", delete=False, mode="w")
        self.tmp.close()
        self.fname = self.tmp.name

    def tearDown(self):
        try:
            os.unlink(self.fname)
        except (PermissionError, FileNotFoundError):
            pass

    def test_write_and_read_single_scenario(self):
        d = ORE.Date(3, ORE.January, 2020)
        s = _make_keyed_scenario(self.factory, d, 0.99500, 1.12000)

        writer = ORE.ScenarioWriter(self.fname)
        writer.writeScenario(s, True)
        writer.close()
        del writer

        reader = ORE.ScenarioFileReader(self.fname, self.factory)
        self.assertTrue(reader.next())
        s_back = reader.scenario()
        self.assertIsNotNone(s_back)

        key_disc = ORE.RiskFactorKey(
            ORE.RiskFactorKey.KeyType_DiscountCurve, "EUR", 0)
        key_fx = ORE.RiskFactorKey(
            ORE.RiskFactorKey.KeyType_FXSpot, "EURUSD", 0)

        self.assertAlmostEqual(s_back.get(key_disc), 0.99500, places=4)
        self.assertAlmostEqual(s_back.get(key_fx),   1.12000, places=4)

    def test_write_and_read_multiple_scenarios(self):
        dates = [
            ORE.Date(3, ORE.January, 2020),
            ORE.Date(6, ORE.January, 2020),
            ORE.Date(7, ORE.January, 2020),
        ]
        values = [0.99500, 0.99490, 0.99480]

        writer = ORE.ScenarioWriter(self.fname)
        for i, (d, v) in enumerate(zip(dates, values)):
            s = _make_keyed_scenario(self.factory, d, v)
            writer.writeScenario(s, i == 0)  # header only on first write
        writer.close()
        del writer

        reader = ORE.ScenarioFileReader(self.fname, self.factory)
        key = ORE.RiskFactorKey(
            ORE.RiskFactorKey.KeyType_DiscountCurve, "EUR", 0)

        read_values = []
        while reader.next():
            read_values.append(reader.scenario().get(key))

        self.assertEqual(len(read_values), 3)
        for got, expected in zip(read_values, values):
            self.assertAlmostEqual(got, expected, places=4)

    def test_write_reset_write(self):
        """Calling reset() on ScenarioWriter should not raise."""
        writer = ORE.ScenarioWriter(self.fname)
        d = ORE.Date(3, ORE.January, 2020)
        s = _make_keyed_scenario(self.factory, d)
        writer.writeScenario(s, True)
        writer.reset()
        writer.close()


if __name__ == "__main__":
    unittest.main()


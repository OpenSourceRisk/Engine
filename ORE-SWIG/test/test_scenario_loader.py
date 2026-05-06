"""Tests for Tier 1 scenario loading infrastructure SWIG bindings.

Covers: ScenarioReader, ScenarioFileReader, ScenarioBufferReader,
        ScenarioLoader, SimpleScenarioLoader, HistoricalScenarioLoader.
"""

import io
import os
import tempfile
import unittest

import ORE


# Minimal CSV content matching ScenarioCSVReader expected format:
# Date,Scenario,Numeraire,<RiskFactorKey>,<RiskFactorKey>,...
_CSV_HEADER = "Date,Scenario,Numeraire,DiscountCurve/EUR/0,FXSpot/EURUSD/0\n"
_CSV_ROW1   = "2020-01-03,1,1.00000,0.99500,1.12000\n"
_CSV_ROW2   = "2020-01-06,1,1.00000,0.99490,1.12100\n"
_CSV_ROW3   = "2020-01-07,1,1.00000,0.99480,1.12200\n"
_CSV_CONTENT = _CSV_HEADER + _CSV_ROW1 + _CSV_ROW2 + _CSV_ROW3


def _make_scenario_factory() -> ORE.ScenarioFactory:
    """Return a SimpleScenarioFactory (no pre-seeded scenario required)."""
    return ORE.SimpleScenarioFactory()


class TestSymbolsAvailable(unittest.TestCase):
    """Check that all Tier 1 symbols are exposed in the ORE module."""

    EXPECTED = [
        "ScenarioReader",
        "ScenarioCSVReader",
        "ScenarioFileReader",
        "ScenarioBufferReader",
        "ScenarioLoader",
        "SimpleScenarioLoader",
        "HistoricalScenarioLoader",
        "SimpleScenarioFactory",
    ]

    def test_tier1_symbols_present(self) -> None:
        for name in self.EXPECTED:
            self.assertTrue(hasattr(ORE, name),
                            msg=f"Missing ORE symbol: {name}")


class TestScenarioBufferReader(unittest.TestCase):
    """ScenarioBufferReader: iterate over an in-memory CSV string."""

    def setUp(self) -> None:
        self.factory = _make_scenario_factory()

    def test_iterates_all_rows(self) -> None:
        reader = ORE.ScenarioBufferReader(_CSV_CONTENT, self.factory)
        count = 0
        while reader.next():
            self.assertNotEqual(reader.date(), ORE.Date())
            s = reader.scenario()
            self.assertIsNotNone(s)
            count += 1
        self.assertEqual(count, 3)

    def test_dates_are_correct(self) -> None:
        reader = ORE.ScenarioBufferReader(_CSV_CONTENT, self.factory)
        expected = [
            ORE.Date(3,  ORE.January, 2020),
            ORE.Date(6,  ORE.January, 2020),
            ORE.Date(7,  ORE.January, 2020),
        ]
        dates = []
        while reader.next():
            dates.append(reader.date())
        self.assertEqual(dates, expected)

    def test_scenario_has_keys(self) -> None:
        reader = ORE.ScenarioBufferReader(_CSV_CONTENT, self.factory)
        reader.next()
        s = reader.scenario()
        # Verify scenario is valid and has data via a known key lookup
        key_discount = ORE.RiskFactorKey(
            ORE.RiskFactorKey.KeyType_DiscountCurve, "EUR", 0)
        self.assertAlmostEqual(s.get(key_discount), 0.99500, places=5)


class TestScenarioFileReader(unittest.TestCase):
    """ScenarioFileReader: read from a temporary CSV file on disk."""

    def setUp(self) -> None:
        self.factory = _make_scenario_factory()
        self.tmp = tempfile.NamedTemporaryFile(
            mode="w", suffix=".csv", delete=False)
        self.tmp.write(_CSV_CONTENT)
        self.tmp.close()
        self.reader = None

    def tearDown(self) -> None:
        # Release C++ file handle before attempting delete
        del self.reader
        try:
            os.unlink(self.tmp.name)
        except PermissionError:
            pass  # File held by C++ runtime; OS will clean temp dir

    def test_reads_from_file(self) -> None:
        self.reader = ORE.ScenarioFileReader(self.tmp.name, self.factory)
        count = 0
        while self.reader.next():
            self.assertNotEqual(self.reader.date(), ORE.Date())
            count += 1
        self.assertEqual(count, 3)

    def test_scenario_values_match(self) -> None:
        self.reader = ORE.ScenarioFileReader(self.tmp.name, self.factory)
        self.reader.next()
        s = self.reader.scenario()
        key_discount = ORE.RiskFactorKey(
            ORE.RiskFactorKey.KeyType_DiscountCurve, "EUR", 0)
        self.assertAlmostEqual(s.get(key_discount), 0.99500, places=5)


class TestSimpleScenarioLoader(unittest.TestCase):
    """SimpleScenarioLoader: construct from a ScenarioBufferReader."""

    def setUp(self) -> None:
        self.factory = _make_scenario_factory()

    def test_default_constructor(self) -> None:
        loader = ORE.SimpleScenarioLoader()
        self.assertIsNotNone(loader)

    def test_construct_from_reader(self) -> None:
        reader = ORE.ScenarioBufferReader(_CSV_CONTENT, self.factory)
        loader = ORE.SimpleScenarioLoader(reader)
        self.assertGreater(loader.numScenarios(), 0)

    def test_samples_count(self) -> None:
        reader = ORE.ScenarioBufferReader(_CSV_CONTENT, self.factory)
        loader = ORE.SimpleScenarioLoader(reader)
        # samples() reflects the number of distinct label groups
        self.assertGreaterEqual(loader.samples(), 1)


class TestHistoricalScenarioLoader(unittest.TestCase):
    """HistoricalScenarioLoader: load by date range and by set of dates."""

    def setUp(self) -> None:
        self.factory = _make_scenario_factory()

    def test_default_constructor(self) -> None:
        loader = ORE.HistoricalScenarioLoader()
        self.assertIsNotNone(loader)

    def test_load_by_date_range(self) -> None:
        reader = ORE.ScenarioBufferReader(_CSV_CONTENT, self.factory)
        start = ORE.Date(3, ORE.January, 2020)
        end   = ORE.Date(7, ORE.January, 2020)
        cal   = ORE.TARGET()
        loader = ORE.HistoricalScenarioLoader(reader, start, end, cal)
        dates = loader.dates()
        self.assertGreater(len(dates), 0)
        self.assertGreaterEqual(dates[0], start)
        self.assertLessEqual(dates[-1], end)

    def test_load_by_date_set(self) -> None:
        reader = ORE.ScenarioBufferReader(_CSV_CONTENT, self.factory)
        date_set = ORE.DateSet()
        date_set.insert(ORE.Date(3, ORE.January, 2020))
        date_set.insert(ORE.Date(6, ORE.January, 2020))
        loader = ORE.HistoricalScenarioLoader(reader, date_set)
        dates = loader.dates()
        self.assertEqual(len(dates), 2)

    def test_get_scenario(self) -> None:
        reader = ORE.ScenarioBufferReader(_CSV_CONTENT, self.factory)
        date_set = ORE.DateSet()
        d = ORE.Date(3, ORE.January, 2020)
        date_set.insert(d)
        loader = ORE.HistoricalScenarioLoader(reader, date_set)
        s = loader.getScenario(d)
        self.assertIsNotNone(s)

    def test_load_from_scenario_vector(self) -> None:
        # Build scenarios via factory so they have the correct QuantExt::Scenario type
        factory = ORE.SimpleScenarioFactory()
        d1 = ORE.Date(3, ORE.January, 2020)
        d2 = ORE.Date(6, ORE.January, 2020)
        s1 = factory.buildScenario(d1, True)
        s2 = factory.buildScenario(d2, True)
        vec = ORE.ScenarioVector()
        vec.append(s1)
        vec.append(s2)
        date_set = ORE.DateSet()
        date_set.insert(d1)
        date_set.insert(d2)
        loader = ORE.HistoricalScenarioLoader(vec, date_set)
        self.assertEqual(len(loader.dates()), 2)


if __name__ == "__main__":
    unittest.main()

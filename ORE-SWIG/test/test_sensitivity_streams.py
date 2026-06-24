"""Tests for SensitivityRecord and SensitivityStream pipeline bindings.

Validates the SWIG bindings for the sensitivity record value type and all
concrete SensitivityStream implementations: SensitivityInMemoryStream,
SensitivityFileStream, SensitivityBufferStream, FilteredSensitivityStream,
and BufferedSensitivityStream.
"""

import os
import tempfile
import unittest

import ORE


class TestSensitivityRecord(unittest.TestCase):
    """Tests for SensitivityRecord value type."""

    def test_default_constructor(self):
        """Default-constructed record is falsy (end-of-stream sentinel)."""
        r = ORE.SensitivityRecord()
        self.assertFalse(bool(r))
        self.assertEqual(r.tradeId, "")
        self.assertEqual(r.delta, 0.0)
        self.assertEqual(r.gamma, 0.0)
        self.assertEqual(r.baseNpv, 0.0)
        self.assertFalse(r.isPar)

    def test_full_constructor(self):
        """Full constructor creates a truthy record with correct fields."""
        key1 = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "USD", 0)
        key2 = ORE.RiskFactorKey()
        r = ORE.SensitivityRecord(
            "trade1", False, key1, "USD/DC/0", 0.0001,
            key2, "", 0.0, "USD", 100.0, 1.5, 0.01
        )
        self.assertTrue(bool(r))
        self.assertEqual(r.tradeId, "trade1")
        self.assertFalse(r.isPar)
        self.assertEqual(r.currency, "USD")
        self.assertAlmostEqual(r.baseNpv, 100.0)
        self.assertAlmostEqual(r.delta, 1.5)
        self.assertAlmostEqual(r.gamma, 0.01)
        self.assertAlmostEqual(r.shift_1, 0.0001)

    def test_is_cross_gamma(self):
        """isCrossGamma returns True only when key_2 is non-default."""
        key1 = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "USD", 0)
        key2 = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "EUR", 1)
        cross = ORE.SensitivityRecord(
            "t1", False, key1, "k1", 0.0001, key2, "k2", 0.0001, "USD", 0.0, 0.0, 0.5
        )
        self.assertTrue(cross.isCrossGamma())

        non_cross = ORE.SensitivityRecord(
            "t1", False, key1, "k1", 0.0001,
            ORE.RiskFactorKey(), "", 0.0, "USD", 0.0, 1.0, 0.0
        )
        self.assertFalse(non_cross.isCrossGamma())

    def test_repr(self):
        """__repr__ returns a readable string."""
        key1 = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "USD", 0)
        r = ORE.SensitivityRecord(
            "trade1", False, key1, "d", 0.0001,
            ORE.RiskFactorKey(), "", 0.0, "USD", 100.0, 1.5, 0.0
        )
        s = repr(r)
        self.assertIn("trade1", s)
        self.assertIn("DiscountCurve", s)


class TestSensitivityRecordVector(unittest.TestCase):
    """Tests for std::vector<SensitivityRecord> template."""

    def test_vector_operations(self):
        """SensitivityRecordVector supports append and iteration."""
        vec = ORE.SensitivityRecordVector()
        key = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_FXSpot, "EURUSD", 0)
        r = ORE.SensitivityRecord(
            "t1", False, key, "fx", 0.01,
            ORE.RiskFactorKey(), "", 0.0, "USD", 50.0, 2.0, 0.0
        )
        vec.append(r)
        vec.append(r)
        self.assertEqual(len(vec), 2)
        self.assertEqual(vec[0].tradeId, "t1")


class TestSensitivityInMemoryStream(unittest.TestCase):
    """Tests for SensitivityInMemoryStream."""

    def _make_record(self, trade_id, delta):
        key = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "USD", 0)
        return ORE.SensitivityRecord(
            trade_id, False, key, "desc", 0.0001,
            ORE.RiskFactorKey(), "", 0.0, "USD", 100.0, delta, 0.0
        )

    def test_empty_stream(self):
        """Empty stream returns falsy record immediately."""
        stream = ORE.SensitivityInMemoryStream()
        rec = stream.next()
        self.assertFalse(bool(rec))

    def test_add_and_iterate(self):
        """Records added via add() are returned by next()."""
        stream = ORE.SensitivityInMemoryStream()
        stream.add(self._make_record("t1", 1.0))
        stream.add(self._make_record("t2", 2.0))

        rec1 = stream.next()
        self.assertEqual(rec1.tradeId, "t1")
        self.assertAlmostEqual(rec1.delta, 1.0)

        rec2 = stream.next()
        self.assertEqual(rec2.tradeId, "t2")

        end = stream.next()
        self.assertFalse(bool(end))

    def test_reset(self):
        """reset() allows re-iteration from the beginning."""
        stream = ORE.SensitivityInMemoryStream()
        stream.add(self._make_record("t1", 1.0))

        stream.next()
        stream.next()  # end
        stream.reset()

        rec = stream.next()
        self.assertEqual(rec.tradeId, "t1")

    def test_iterator_protocol(self):
        """SensitivityStream supports Python iterator protocol (for-in, next())."""
        stream = ORE.SensitivityInMemoryStream()
        stream.add(self._make_record("t1", 1.0))
        stream.add(self._make_record("t2", 2.0))

        records = [r.tradeId for r in stream]
        self.assertEqual(records, ["t1", "t2"])

    def test_read_all(self):
        """readAll() returns all records in a list."""
        stream = ORE.SensitivityInMemoryStream()
        stream.add(self._make_record("t1", 1.0))
        stream.add(self._make_record("t2", 2.0))

        records = stream.readAll()
        self.assertEqual(len(records), 2)
        self.assertEqual(records[0].tradeId, "t1")
        self.assertEqual(records[1].tradeId, "t2")


class TestSensitivityBufferStream(unittest.TestCase):
    """Tests for SensitivityBufferStream (in-memory CSV parsing)."""

    CSV_DATA = (
        "# TradeId,IsPar,Factor_1,ShiftSize_1,Factor_2,ShiftSize_2,"
        "Currency,BaseNPV,Delta,Gamma\n"
        "trade1,false,DiscountCurve/USD/1,0.0001,,,USD,100.5,1.25,0.001\n"
        "trade2,false,DiscountCurve/EUR/2,0.0001,,,EUR,200.0,-0.5,0.0\n"
    )

    def test_parse_csv_buffer(self):
        """Parses CSV from an in-memory string."""
        stream = ORE.SensitivityBufferStream(self.CSV_DATA)
        rec1 = stream.next()
        self.assertEqual(rec1.tradeId, "trade1")
        self.assertAlmostEqual(rec1.delta, 1.25)

        rec2 = stream.next()
        self.assertEqual(rec2.tradeId, "trade2")
        self.assertAlmostEqual(rec2.delta, -0.5)

        end = stream.next()
        self.assertFalse(bool(end))

    def test_reset(self):
        """reset() re-reads from beginning."""
        stream = ORE.SensitivityBufferStream(self.CSV_DATA)
        stream.next()
        stream.next()
        stream.reset()
        rec = stream.next()
        self.assertEqual(rec.tradeId, "trade1")


class TestSensitivityFileStream(unittest.TestCase):
    """Tests for SensitivityFileStream (CSV file reading)."""

    CSV_DATA = (
        "# TradeId,IsPar,Factor_1,ShiftSize_1,Factor_2,ShiftSize_2,"
        "Currency,BaseNPV,Delta,Gamma\n"
        "trade_a,false,YieldCurve/USD/0,0.0001,,,USD,50.0,0.75,0.0\n"
    )

    def test_read_from_file(self):
        """Reads sensitivity records from a CSV file."""
        tmpfile = tempfile.NamedTemporaryFile(
            mode="w", suffix=".csv", delete=False
        )
        tmpfile.write(self.CSV_DATA)
        tmpfile.close()

        try:
            stream = ORE.SensitivityFileStream(tmpfile.name)
            rec = stream.next()
            self.assertEqual(rec.tradeId, "trade_a")
            self.assertAlmostEqual(rec.delta, 0.75)

            end = stream.next()
            self.assertFalse(bool(end))

            stream.reset()
            rec2 = stream.next()
            self.assertEqual(rec2.tradeId, "trade_a")
            del stream
        finally:
            os.unlink(tmpfile.name)


class TestFilteredSensitivityStream(unittest.TestCase):
    """Tests for FilteredSensitivityStream."""

    def _make_stream(self):
        """Create a stream with records of varying delta magnitudes."""
        stream = ORE.SensitivityInMemoryStream()
        key = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "USD", 0)
        for trade_id, delta in [("big", 5.0), ("small", 0.001), ("medium", 1.5)]:
            r = ORE.SensitivityRecord(
                trade_id, False, key, "d", 0.0001,
                ORE.RiskFactorKey(), "", 0.0, "USD", 100.0, delta, 0.0
            )
            stream.add(r)
        return stream

    def test_single_threshold(self):
        """Records below threshold are filtered out."""
        stream = self._make_stream()
        filtered = ORE.FilteredSensitivityStream(stream, 1.0)

        rec1 = filtered.next()
        self.assertEqual(rec1.tradeId, "big")

        rec2 = filtered.next()
        self.assertEqual(rec2.tradeId, "medium")

        end = filtered.next()
        self.assertFalse(bool(end))

    def test_dual_threshold(self):
        """Separate delta and gamma thresholds."""
        stream = self._make_stream()
        filtered = ORE.FilteredSensitivityStream(stream, 2.0, 0.0)

        rec1 = filtered.next()
        self.assertEqual(rec1.tradeId, "big")

        end = filtered.next()
        self.assertFalse(bool(end))

    def test_polymorphic_input(self):
        """Accepts any SensitivityStream subclass."""
        csv = (
            "# h\n"
            "t1,false,DiscountCurve/USD/0,0.0001,,,USD,100,10.0,0.0\n"
        )
        buf_stream = ORE.SensitivityBufferStream(csv)
        filtered = ORE.FilteredSensitivityStream(buf_stream, 1.0)
        rec = filtered.next()
        self.assertEqual(rec.tradeId, "t1")


class TestBufferedSensitivityStream(unittest.TestCase):
    """Tests for BufferedSensitivityStream."""

    def test_buffers_and_resets(self):
        """Buffered stream reads all on first pass, resets from buffer."""
        stream = ORE.SensitivityInMemoryStream()
        key = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_FXSpot, "EURUSD", 0)
        for i in range(3):
            r = ORE.SensitivityRecord(
                f"t{i}", False, key, "fx", 0.01,
                ORE.RiskFactorKey(), "", 0.0, "USD", 10.0, float(i), 0.0
            )
            stream.add(r)

        buffered = ORE.BufferedSensitivityStream(stream)

        # First pass
        records = []
        while True:
            rec = buffered.next()
            if not rec:
                break
            records.append(rec.tradeId)
        self.assertEqual(records, ["t0", "t1", "t2"])

        # Reset re-reads from buffer (no stream dependency)
        buffered.reset()
        rec = buffered.next()
        self.assertEqual(rec.tradeId, "t0")

    def test_chained_streams(self):
        """BufferedSensitivityStream wrapping a FilteredSensitivityStream."""
        stream = ORE.SensitivityInMemoryStream()
        key = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "USD", 0)
        for trade_id, delta in [("big", 5.0), ("tiny", 0.001)]:
            r = ORE.SensitivityRecord(
                trade_id, False, key, "d", 0.0001,
                ORE.RiskFactorKey(), "", 0.0, "USD", 100.0, delta, 0.0
            )
            stream.add(r)

        filtered = ORE.FilteredSensitivityStream(stream, 1.0)
        buffered = ORE.BufferedSensitivityStream(filtered)

        rec = buffered.next()
        self.assertEqual(rec.tradeId, "big")
        end = buffered.next()
        self.assertFalse(bool(end))


class TestDecomposedSensitivityStream(unittest.TestCase):
    """Tests for DecomposedSensitivityStream."""

    def test_instantiation(self):
        """Verify that DecomposedSensitivityStream can be instantiated."""
        stream = ORE.SensitivityInMemoryStream()
        portfolio = ORE.Portfolio()
        decomposed = ORE.DecomposedSensitivityStream(stream, "USD", portfolio)
        self.assertTrue(hasattr(decomposed, "next"))
        self.assertTrue(hasattr(decomposed, "reset"))


class TestSensitivityReportStream(unittest.TestCase):
    """Tests for SensitivityReportStream."""

    def test_instantiation(self):
        """Verify that SensitivityReportStream can be instantiated."""
        report = ORE.InMemoryReport()
        report_stream = ORE.SensitivityReportStream(report)
        self.assertTrue(hasattr(report_stream, "next"))
        self.assertTrue(hasattr(report_stream, "reset"))


if __name__ == "__main__":
    unittest.main()

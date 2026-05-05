"""
 Copyright (C) 2025 Quaternion Risk Management Ltd
 All rights reserved.
"""

from ORE import *
import unittest
import os
import tempfile


class InMemoryReportTest(unittest.TestCase):

    def test_create_and_populate(self):
        """InMemoryReport can be constructed, populated, and inspected."""
        report = InMemoryReport()
        report.addColumnString("TradeId")
        report.addColumnReal("NPV", 2)
        report.addColumnDate("Maturity")

        report.nextRow()
        report.addString("T001")
        report.addReal(1234567.89)
        report.addDate(Date(15, June, 2030))

        report.nextRow()
        report.addString("T002")
        report.addReal(-500000.0)
        report.addDate(Date(1, January, 2028))

        report.end()

        self.assertEqual(report.columns(), 3)
        self.assertEqual(report.rows(), 2)
        self.assertEqual(report.header(0), "TradeId")
        self.assertEqual(report.header(1), "NPV")
        self.assertEqual(report.header(2), "Maturity")

    def test_has_header(self):
        report = InMemoryReport()
        report.addColumnString("Id")
        report.end()
        self.assertTrue(report.hasHeader("Id"))
        self.assertFalse(report.hasHeader("Missing"))

    def test_add_size_column(self):
        report = InMemoryReport()
        report.addColumnSize("Count")
        report.nextRow()
        report.addSize(42)
        report.end()
        self.assertEqual(report.columns(), 1)
        self.assertEqual(report.rows(), 1)

    def test_add_period_column(self):
        report = InMemoryReport()
        report.addColumnPeriod("Tenor")
        report.nextRow()
        report.addPeriod(Period(3, Months))
        report.end()
        self.assertEqual(report.columns(), 1)
        self.assertEqual(report.rows(), 1)

    def test_to_file(self):
        """InMemoryReport.toFile writes a CSV with header and data rows."""
        report = InMemoryReport()
        report.addColumnString("Id")
        report.addColumnReal("Value", 4)
        report.nextRow()
        report.addString("A")
        report.addReal(1.5)
        report.end()

        with tempfile.NamedTemporaryFile(suffix=".csv", delete=False) as f:
            fname = f.name
        try:
            report.toFile(fname)
            with open(fname) as f:
                lines = [l for l in f.readlines() if l.strip()]
            # expect comment header line + data line
            self.assertGreaterEqual(len(lines), 2)
            combined = "".join(lines)
            self.assertIn("Id", combined)
            self.assertIn("A", combined)
        finally:
            os.unlink(fname)

    def test_python_dispatch_add(self):
        """Unified add() dispatch works for all supported Python types."""
        report = InMemoryReport()
        report.addColumnString("Name")
        report.addColumnReal("Val")
        report.addColumnSize("Count")
        report.addColumnDate("Dt")
        report.addColumnPeriod("Tenor")

        report.nextRow()
        report.add("hello")
        report.add(3.14)
        report.add(7)
        report.add(Date(1, January, 2025))
        report.add(Period(6, Months))
        report.end()

        self.assertEqual(report.rows(), 1)

    def test_python_dispatch_add_column(self):
        """Unified addColumn() dispatch works for all col types."""
        report = InMemoryReport()
        report.addColumn("A", "string")
        report.addColumn("B", "real", 4)
        report.addColumn("C", "size")
        report.addColumn("D", "date")
        report.addColumn("E", "period")
        report.end()
        self.assertEqual(report.columns(), 5)

    def test_plain_inmemoryreport_from_inmemoryreport(self):
        """PlainInMemoryReport wraps InMemoryReport and exposes typed accessors."""
        report = InMemoryReport()
        report.addColumnReal("X")
        report.nextRow()
        report.addReal(42.0)
        report.end()

        plain = PlainInMemoryReport(report)
        self.assertEqual(plain.rows(), 1)
        self.assertAlmostEqual(plain.dataAsReal(0)[0], 42.0)

    def test_empty_report(self):
        """An empty report has 0 columns and 0 rows."""
        report = InMemoryReport()
        report.end()
        self.assertEqual(report.columns(), 0)
        self.assertEqual(report.rows(), 0)


class CSVFileReportTest(unittest.TestCase):

    def test_write_and_read(self):
        """CSVFileReport writes header and data to a file."""
        with tempfile.NamedTemporaryFile(suffix=".csv", delete=False) as f:
            fname = f.name
        try:
            report = CSVFileReport(fname)
            report.addColumnString("Name")
            report.addColumnReal("Price", 4)
            report.nextRow()
            report.addString("Widget")
            report.addReal(9.9999)
            report.end()

            with open(fname) as f:
                content = f.read()
            self.assertIn("Widget", content)
            self.assertIn("Name", content)
        finally:
            os.unlink(fname)

    def test_python_dispatch_add(self):
        """Unified add() dispatch works on CSVFileReport."""
        with tempfile.NamedTemporaryFile(suffix=".csv", delete=False) as f:
            fname = f.name
        try:
            report = CSVFileReport(fname)
            report.addColumnString("S")
            report.addColumnReal("R")
            report.nextRow()
            report.add("test")
            report.add(1.23)
            report.end()
            with open(fname) as f:
                content = f.read()
            self.assertIn("test", content)
        finally:
            os.unlink(fname)


if __name__ == "__main__":
    unittest.main()

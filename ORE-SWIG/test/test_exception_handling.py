"""
Exception mapping tests for ORE Python bindings.
"""

import unittest

import ORE


class ExceptionHandlingTest(unittest.TestCase):
    """Validate exception translation behavior exposed by SWIG."""

    def test_runtime_error_propagates(self) -> None:
        """Invalid parser input should propagate as RuntimeError."""
        with self.assertRaises(RuntimeError):
            ORE.parseDate("not-a-date")

    def test_index_error_for_out_of_range_access(self) -> None:
        """Out-of-range STL access should raise IndexError in Python."""
        values = ORE.DateVector()
        with self.assertRaises(IndexError):
            _ = values[0]


if __name__ == "__main__":
    unittest.main()

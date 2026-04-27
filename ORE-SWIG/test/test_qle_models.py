"""Smoke tests for QuantExt model bindings."""

from ORE import *
import unittest


class QuantExtModelBindingSmokeTest(unittest.TestCase):
    def test_model_symbols_available(self):
        """Ensure key model classes are exposed in the module."""
        self.assertTrue(hasattr(__import__("ORE"), "LinkableCalibratedModel"))
        self.assertTrue(hasattr(__import__("ORE"), "Parametrization"))
        self.assertTrue(hasattr(__import__("ORE"), "IrModel"))
        self.assertTrue(hasattr(__import__("ORE"), "LinearGaussMarkovModel"))
        self.assertTrue(hasattr(__import__("ORE"), "HwModel"))
        self.assertTrue(hasattr(__import__("ORE"), "CrossAssetModel"))
        self.assertTrue(hasattr(__import__("ORE"), "HullWhiteBucketing"))

    def test_hull_white_bucketing_constructs(self):
        """Build a HullWhiteBucketing instance and inspect basic state."""
        hwb = HullWhiteBucketing(-2.0, 2.0, 8)
        self.assertGreaterEqual(hwb.buckets(), 2)

    def test_transition_matrix_utilities_are_callable(self):
        """Call transition matrix utility functions from bindings."""
        matrix = Matrix(2, 2, 0.0)
        matrix[0][0] = 0.9
        matrix[0][1] = 0.1
        matrix[1][0] = 0.2
        matrix[1][1] = 0.8

        sanitiseTransitionMatrix(matrix)
        checkTransitionMatrix(matrix)
        try:
            generator_matrix = generator(matrix, 1.0)
            self.assertEqual(generator_matrix.rows(), 2)
            self.assertEqual(generator_matrix.columns(), 2)
        except RuntimeError as error:
            self.assertIn("Logm()", str(error))


if __name__ == "__main__":
    unittest.main()

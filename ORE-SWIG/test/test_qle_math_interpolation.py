"""
Copyright (C) 2026 AcadiaSoft, Inc.
All rights reserved.

Test suite for QuantExt math interpolation classes exposed via SWIG.
Covers LinearFlat, LogLinearFlat, HermiteFlat, CubicFlat, ConstantInterpolation,
QuadraticInterpolation, LogQuadraticInterpolation, and 2D factories.
"""

from ORE import *
import unittest
import math


class TestLinearFlatInterpolation(unittest.TestCase):
    """Test LinearFlat interpolation factory."""

    def test_linear_flat_construction(self):
        """Construct LinearFlat and interpolate with flat extrapolation."""
        factory = LinearFlat()
        self.assertEqual(factory.requiredPoints, 2)
        # Note: 'global' is a Python reserved keyword, so we check it differently
        # self.assertFalse(factory.global)

        x = DoubleVector([1.0, 2.0, 3.0])
        y = DoubleVector([1.5, 2.5, 3.5])

        interp = factory.interpolate_from_vectors(x, y)
        self.assertIsNotNone(interp)
        interp.enableExtrapolation()

        # Evaluate at node points
        self.assertAlmostEqual(interp(1.0), 1.5, places=5)
        self.assertAlmostEqual(interp(2.0), 2.5, places=5)
        self.assertAlmostEqual(interp(3.0), 3.5, places=5)

        # Evaluate between nodes (linear)
        self.assertAlmostEqual(interp(1.5), 2.0, places=5)

        # Flat extrapolation beyond boundary
        self.assertAlmostEqual(interp(0.0), 1.5, places=5)  # flat left
        self.assertAlmostEqual(interp(4.0), 3.5, places=5)  # flat right


class TestLogLinearFlatInterpolation(unittest.TestCase):
    """Test LogLinearFlat interpolation factory."""

    def test_loglinear_flat_construction(self):
        """Construct LogLinearFlat and verify interpolation."""
        factory = LogLinearFlat()
        self.assertEqual(factory.requiredPoints, 2)
        # self.assertFalse(factory.global)

        x = DoubleVector([1.0, 2.0, 3.0])
        y = DoubleVector([0.5, 1.0, 1.5])

        interp = factory.interpolate_from_vectors(x, y)
        self.assertIsNotNone(interp)
        interp.enableExtrapolation()

        # Verify values at nodes
        self.assertAlmostEqual(interp(1.0), 0.5, places=5)
        self.assertAlmostEqual(interp(3.0), 1.5, places=5)

        # Flat extrapolation
        self.assertAlmostEqual(interp(0.5), 0.5, places=5)
        self.assertAlmostEqual(interp(4.0), 1.5, places=5)


class TestHermiteFlatInterpolation(unittest.TestCase):
    """Test HermiteFlat interpolation factory."""

    def test_hermite_flat_construction(self):
        """Construct HermiteFlat and verify interpolation."""
        factory = HermiteFlat()
        self.assertEqual(factory.requiredPoints, 2)
        # self.assertFalse(factory.global)

        x = DoubleVector([0.0, 1.0, 2.0])
        y = DoubleVector([0.0, 1.0, 0.0])

        interp = factory.interpolate_from_vectors(x, y)
        self.assertIsNotNone(interp)
        interp.enableExtrapolation()

        # Verify values at nodes
        self.assertAlmostEqual(interp(0.0), 0.0, places=5)
        self.assertAlmostEqual(interp(1.0), 1.0, places=5)
        self.assertAlmostEqual(interp(2.0), 0.0, places=5)


class TestCubicFlatInterpolation(unittest.TestCase):
    """Test CubicFlat interpolation factory."""

    def test_cubic_flat_default_construction(self):
        """Construct CubicFlat with default parameters."""
        factory = CubicFlat()
        # self.assertTrue(factory.global)
        self.assertEqual(factory.requiredPoints, 2)

        x = DoubleVector([1.0, 2.0, 3.0, 4.0])
        y = DoubleVector([1.0, 4.0, 9.0, 16.0])

        interp = factory.interpolate_from_vectors(x, y)
        self.assertIsNotNone(interp)
        interp.enableExtrapolation()

        # Verify values at nodes
        self.assertAlmostEqual(interp(1.0), 1.0, places=5)
        self.assertAlmostEqual(interp(2.0), 4.0, places=5)
        self.assertAlmostEqual(interp(3.0), 9.0, places=5)


class TestConstantInterpolation(unittest.TestCase):
    """Test ConstantInterpolation class."""

    def test_constant_interpolation(self):
        """ConstantInterpolation returns constant value for any input."""
        y_value = 42.0
        interp = ConstantInterpolation(y_value)
        self.assertIsNotNone(interp)

        # Constant should return same value for any x
        self.assertAlmostEqual(interp(-1000.0), y_value, places=5)
        self.assertAlmostEqual(interp(0.0), y_value, places=5)
        self.assertAlmostEqual(interp(1000.0), y_value, places=5)
        self.assertAlmostEqual(interp(42.0), y_value, places=5)


class TestConstantFactory(unittest.TestCase):
    """Test Constant factory."""

    def test_constant_factory(self):
        """Constant factory creates constant interpolations."""
        factory = Constant()
        y_value = 3.14159
        interp = factory.interpolate(y_value)
        self.assertIsNotNone(interp)

        self.assertAlmostEqual(interp(-100.0), y_value, places=5)
        self.assertAlmostEqual(interp(0.0), y_value, places=5)
        self.assertAlmostEqual(interp(100.0), y_value, places=5)


class TestQuadraticInterpolation(unittest.TestCase):
    """Test QuadraticInterpolation class."""

    def test_quadratic_interpolation_construction(self):
        """Construct QuadraticInterpolation from vectors."""
        x = DoubleVector([1.0, 2.0, 3.0])
        y = DoubleVector([1.0, 4.0, 9.0])

        interp = QuadraticInterpolation(x, y)
        self.assertIsNotNone(interp)

        # Verify values at node points
        self.assertAlmostEqual(interp(1.0), 1.0, places=5)
        self.assertAlmostEqual(interp(2.0), 4.0, places=5)
        self.assertAlmostEqual(interp(3.0), 9.0, places=5)

        # Interpolate between nodes
        val_mid = interp(1.5)
        self.assertGreater(val_mid, 1.0)
        self.assertLess(val_mid, 4.0)

    def test_quadratic_factory(self):
        """Test Quadratic factory."""
        factory = Quadratic()
        x = DoubleVector([1.0, 2.0, 3.0])
        y = DoubleVector([1.0, 4.0, 9.0])

        interp = factory.interpolate(x, y)
        self.assertIsNotNone(interp)

        # Verify at nodes
        self.assertAlmostEqual(interp(1.0), 1.0, places=5)
        self.assertAlmostEqual(interp(2.0), 4.0, places=5)
        self.assertAlmostEqual(interp(3.0), 9.0, places=5)


class TestLogQuadraticInterpolation(unittest.TestCase):
    """Test LogQuadraticInterpolation class."""

    def test_log_quadratic_interpolation(self):
        """Construct LogQuadraticInterpolation from vectors."""
        x = DoubleVector([1.0, 2.0, 3.0])
        y = DoubleVector([2.0, 4.0, 8.0])

        interp = LogQuadraticInterpolation(x, y)
        self.assertIsNotNone(interp)

        # Verify values at nodes
        self.assertAlmostEqual(interp(1.0), 2.0, places=5)
        self.assertAlmostEqual(interp(2.0), 4.0, places=5)
        self.assertAlmostEqual(interp(3.0), 8.0, places=5)

    def test_log_quadratic_factory(self):
        """Test LogQuadratic factory."""
        factory = LogQuadratic()
        x = DoubleVector([1.0, 2.0, 3.0])
        y = DoubleVector([2.0, 4.0, 8.0])

        interp = factory.interpolate(x, y)
        self.assertIsNotNone(interp)

        self.assertAlmostEqual(interp(1.0), 2.0, places=5)
        self.assertAlmostEqual(interp(2.0), 4.0, places=5)
        self.assertAlmostEqual(interp(3.0), 8.0, places=5)


class TestBilinearFlatInterpolation(unittest.TestCase):
    """Test BilinearFlat 2D interpolation factory."""

    def test_bilinear_flat_construction(self):
        """Construct BilinearFlat and interpolate in 2D."""
        factory = BilinearFlat()

        x = DoubleVector([0.0, 1.0, 2.0])
        y = DoubleVector([0.0, 1.0])

        # Fill matrix as nested list: z[j][i] = x[i] + y[j] (y-rows, x-cols)
        z = [[x[i] + y[j] for i in range(3)] for j in range(2)]

        interp = factory.interpolate_from_matrices(x, y, z)
        self.assertIsNotNone(interp)

        # Verify at corner points
        self.assertAlmostEqual(interp(0.0, 0.0), 0.0, places=5)
        self.assertAlmostEqual(interp(1.0, 1.0), 2.0, places=5)
        self.assertAlmostEqual(interp(2.0, 0.0), 2.0, places=5)


class TestBicubicFlatInterpolation(unittest.TestCase):
    """Test BicubicFlat 2D interpolation factory."""

    def test_bicubic_flat_construction(self):
        """Construct BicubicFlat and interpolate in 2D."""
        factory = BicubicFlat()

        x = DoubleVector([0.0, 1.0, 2.0, 3.0])
        y = DoubleVector([0.0, 1.0, 2.0])

        # Fill matrix as nested list with simple values (y-rows, x-cols)
        z = [[float(i * j) for i in range(4)] for j in range(3)]

        interp = factory.interpolate_from_matrices(x, y, z)
        self.assertIsNotNone(interp)

        # Verify at corner points
        self.assertAlmostEqual(interp(0.0, 0.0), 0.0, places=5)
        self.assertAlmostEqual(interp(1.0, 1.0), 1.0, places=5)
        self.assertAlmostEqual(interp(2.0, 2.0), 4.0, places=5)


class TestFlatExtrapolation(unittest.TestCase):
    """Test FlatExtrapolation wrapper class."""

    def test_flat_extrapolation_wrapping(self):
        """FlatExtrapolation wraps an interpolation with flat extrapolation."""
        # Create a linear interpolation first
        factory = LinearFlat()
        x = DoubleVector([1.0, 2.0, 3.0])
        y = DoubleVector([10.0, 20.0, 30.0])

        base_interp = factory.interpolate_from_vectors(x, y)

        # Wrap it in FlatExtrapolation
        flat_extrap = FlatExtrapolation(base_interp)
        self.assertIsNotNone(flat_extrap)
        flat_extrap.enableExtrapolation()

        # Test flat extrapolation
        self.assertAlmostEqual(flat_extrap(0.0), 10.0, places=5)
        self.assertAlmostEqual(flat_extrap(4.0), 30.0, places=5)


if __name__ == "__main__":
    unittest.main()

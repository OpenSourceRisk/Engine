"""
Copyright (C) 2026 AcadiaSoft, Inc.
All rights reserved.
"""

from ORE import *
import unittest


class TestQleMathRandomVariable(unittest.TestCase):
    def test_randomvariable_arithmetic_and_stats(self):
        rv = RandomVariable(8, 0.5)
        self.assertTrue(rv.deterministic())
        self.assertEqual(rv.size(), 8)
        self.assertAlmostEqual(expectation(rv)[0], 0.5, places=10)
        self.assertAlmostEqual(variance(rv)[0], 0.0, places=10)

        rv2 = RandomVariable(8, 1.5)
        rv3 = rv + 2.0
        rv4 = rv * rv2
        self.assertAlmostEqual(rv3[0], 2.5, places=10)
        self.assertAlmostEqual(rv4[0], 0.75, places=10)

    def test_filter_and_conditionals(self):
        base = RandomVariable(6, 2.0)
        mask = Filter(6, False)
        for i in range(3):
            mask[i] = True

        masked = applyFilter(base, mask)
        inv_masked = applyInverseFilter(base, mask)
        self.assertAlmostEqual(masked[0], 2.0, places=10)
        self.assertAlmostEqual(masked[5], 0.0, places=10)
        self.assertAlmostEqual(inv_masked[0], 0.0, places=10)
        self.assertAlmostEqual(inv_masked[5], 2.0, places=10)

    def test_black_wrapper(self):
        n = 16
        omega = RandomVariable(n, 1.0)
        t = RandomVariable(n, 1.0)
        strike = RandomVariable(n, 100.0)
        forward = RandomVariable(n, 100.0)
        vol = RandomVariable(n, 0.20)

        price = black(omega, t, strike, forward, vol)
        self.assertGreaterEqual(price[0], 0.0)
        self.assertLessEqual(price[0], forward[0])

    def test_lsm_basis_helpers(self):
        rv = RandomVariable(10, 1.2)
        basis = RandomVariableLsmBasisSystem.evaluatePathBasis(2, LsmBasisSystem.Monomial, rv)
        self.assertGreaterEqual(len(basis), 1)
        self.assertEqual(basis[0].size(), rv.size())

        multi_basis = RandomVariableLsmBasisSystem.evaluateMultiPathBasis(
            2, 2, LsmBasisSystem.Monomial, RandomVariableVector([rv, rv + 1.0])
        )
        self.assertGreaterEqual(len(multi_basis), 1)


if __name__ == "__main__":
    unittest.main()

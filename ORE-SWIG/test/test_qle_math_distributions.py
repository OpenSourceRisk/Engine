"""
Copyright (C) 2026 AcadiaSoft, Inc.
All rights reserved.
"""

from ORE import *
import unittest


class TestQleMathDistributions(unittest.TestCase):
    """Coverage for QuantExt distribution wrappers in qle_math.i."""

    @staticmethod
    def _total_probability(dd):
        """Return total probability mass in discrete distribution."""
        total = 0.0
        for i in range(dd.size()):
            total += dd.probability(i)
        return total

    def test_distributionpair_and_discrete_distribution(self):
        """Build distribution pairs and verify discrete accessors."""
        pairs = DistributionPairVector(
            [Distributionpair(0.0, 0.4), Distributionpair(1.0, 0.6)]
        )
        dd = DiscreteDistribution(pairs)
        self.assertEqual(dd.size(), 2)
        self.assertAlmostEqual(dd.get(0).x, 0.0, places=10)
        self.assertAlmostEqual(dd.get(0).probability, 0.4, places=10)
        self.assertAlmostEqual(dd.data(1), 1.0, places=10)
        self.assertAlmostEqual(self._total_probability(dd), 1.0, places=10)

    def test_mdd_convolve_and_expectation(self):
        """Exercise core MDD static methods with small deterministic inputs."""
        a = DiscreteDistribution(DoubleVector([0.0, 1.0]), DoubleVector([0.5, 0.5]))
        b = DiscreteDistribution(DoubleVector([1.0, 2.0]), DoubleVector([0.5, 0.5]))
        c = MDD.convolve(a, b, 4)
        self.assertGreater(c.size(), 0)
        self.assertAlmostEqual(self._total_probability(c), 1.0, places=6)
        self.assertAlmostEqual(MDD.expectation(a), 0.5, places=10)
        self.assertAlmostEqual(MDD.expectation(b), 1.5, places=10)
        self.assertAlmostEqual(MDD.expectation(c), 2.0, places=6)

    def test_bucketed_distribution_add_cdf_and_operators(self):
        """Verify bucketed add, CDF queries, rebucketing helpers and operators."""
        dd = DiscreteDistribution(DoubleVector([0.25, 1.25]), DoubleVector([0.5, 0.5]))
        bd = BucketedDistribution(0.0, 2.0, 2)
        bd.add(dd)

        self.assertEqual(bd.numberBuckets(), 2)
        cdf_mid = bd.cumulativeProbability(1.0)
        self.assertGreaterEqual(cdf_mid, 0.0)
        self.assertLessEqual(cdf_mid, 1.0)
        self.assertAlmostEqual(bd.inverseCumulativeProbability(0.5), 1.0, places=6)

        disc = bd.createDiscrete()
        self.assertEqual(disc.size(), 2)

        bd2 = BucketedDistribution(0.0, 2.0, 2, 0.25)
        sum_dist = bd + bd2
        scaled_dist = 2.0 * bd2
        self.assertEqual(sum_dist.numberBuckets(), bd.numberBuckets())
        self.assertEqual(scaled_dist.numberBuckets(), bd2.numberBuckets())


if __name__ == "__main__":
    unittest.main()

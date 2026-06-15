"""
Copyright (C) 2026 AcadiaSoft, Inc.
All rights reserved.
"""

from ORE import *
import unittest


class TestQleMathStats(unittest.TestCase):
    """Coverage for QuantExt statistical utilities in qle_math.i."""

    def test_covariance_salvage_and_psd_fix(self):
        """Check no-op salvage and spectral salvage outputs."""
        corr = [[1.0, 0.25], [0.25, 1.0]]
        no_salvage = NoCovarianceSalvage()
        p_no = no_salvage.salvageMatrix(corr)
        s_no = no_salvage.salvageSqrt(corr)
        self.assertAlmostEqual(p_no[0][0], 1.0, places=10)
        self.assertAlmostEqual(p_no[0][1], 0.25, places=10)
        self.assertEqual(s_no.rows(), 0)

        near_psd = [[1.0, 1.2], [1.2, 1.0]]
        spectral = SpectralCovarianceSalvage()
        p_spec = spectral.salvageMatrix(near_psd)
        l_spec = spectral.salvageSqrt(near_psd)
        self.assertGreaterEqual(p_spec[0][0], 0.0)
        self.assertGreaterEqual(p_spec[1][1], 0.0)
        self.assertAlmostEqual(p_spec[0][1], p_spec[1][0], places=10)
        self.assertEqual(l_spec.rows(), 2)

    def test_logm_expm_and_trace(self):
        """Verify support flags and basic matrix functions."""
        self.assertIsInstance(supports_Logm(), bool)
        self.assertIsInstance(supports_Expm(), bool)
        self.assertAlmostEqual(Trace([[1.0, 0.0], [0.0, 1.0]]), 2.0, places=10)

        if supports_Logm() and supports_Expm():
            base = [[1.1, 0.1], [0.1, 0.9]]
            roundtrip = Logm(Expm(base))
            self.assertAlmostEqual(roundtrip[0][0], base[0][0], places=6)
            self.assertAlmostEqual(roundtrip[0][1], base[0][1], places=6)
            self.assertAlmostEqual(roundtrip[1][0], base[1][0], places=6)
            self.assertAlmostEqual(roundtrip[1][1], base[1][1], places=6)

    def test_fill_incomplete_matrix_wrapper(self):
        """Verify copy-returning fillIncompleteMatrix helper."""
        blank = -999.0
        mat = [[1.0, blank, 3.0], [2.0, 4.0, 6.0]]
        filled = fillIncompleteMatrix(mat, True, blank)
        self.assertNotEqual(filled[0][1], blank)
        self.assertAlmostEqual(filled[0][1], 2.0, places=10)

    def test_stabilised_glls_diagnostics(self):
        """Fit linear data and inspect diagnostics."""
        x = DoubleVector([1.0, 2.0, 3.0, 4.0, 5.0])
        y = DoubleVector([3.0, 5.0, 7.0, 9.0, 11.0])
        glls = StabilisedGLLS(x, y, 1, StabilisedGLLS.MeanStdDev)
        self.assertEqual(glls.dim(), 2)
        self.assertEqual(glls.size(), 5)
        self.assertEqual(len(glls.transformedCoefficients()), 2)
        self.assertEqual(len(glls.transformedResiduals()), 5)
        self.assertGreater(glls.yMultiplier(), 0.0)

    def test_nadaraya_watson_gaussian(self):
        """Verify Gaussian-kernel Nadaraya-Watson wrapper."""
        x = DoubleVector([0.0, 1.0, 2.0, 3.0])
        y = DoubleVector([0.0, 2.0, 4.0, 6.0])
        nw = NadarayaWatson(x, y, 0.2)
        self.assertAlmostEqual(nw(0.0), 0.0, places=4)
        self.assertAlmostEqual(nw(2.0), 4.0, places=6)
        self.assertGreaterEqual(nw.standardDeviation(1.5), 0.0)


if __name__ == "__main__":
    unittest.main()

"""Tests for SensitivityCube full API and SensitivityAnalysis.sensiCubes() accessor.

Validates the SWIG bindings for the expanded SensitivityCube class (delta, gamma,
crossGamma, theta, factor enumeration, shift sizes) and the sensiCubes()/sensiCube()
accessors on SensitivityAnalysis.
"""

import unittest

import ORE


class TestSensitivityCubeSymbols(unittest.TestCase):
    """Verify all newly exposed SensitivityCube symbols are available."""

    def test_sensitivity_cube_methods_available(self):
        """Assert that key SensitivityCube methods are exposed."""
        required_methods = [
            "hasTrade",
            "npv",
            "delta",
            "gamma",
            "crossGamma",
            "theta",
            "targetShiftSize",
            "actualShiftSize",
            "shiftScheme",
            "factors",
            "upFactors",
            "downFactors",
            "tradeIdx",
            "relevantRiskFactors",
            "factorDescription",
            "allDeltas",
            "allGammas",
        ]
        for name in required_methods:
            self.assertTrue(
                hasattr(ORE.SensitivityCube, name),
                msg=f"Missing SensitivityCube method: {name}",
            )

    def test_sensitivity_analysis_cube_accessors(self):
        """Assert sensiCubes() and sensiCube() are exposed on SensitivityAnalysis."""
        self.assertTrue(
            hasattr(ORE.SensitivityAnalysis, "sensiCubes"),
            msg="Missing SensitivityAnalysis.sensiCubes",
        )
        self.assertTrue(
            hasattr(ORE.SensitivityAnalysis, "sensiCube"),
            msg="Missing SensitivityAnalysis.sensiCube",
        )

    def test_factor_data_struct_available(self):
        """Assert the nested FactorData struct is exposed."""
        self.assertTrue(
            hasattr(ORE, "SensitivityCubeFactorData"),
            msg="Missing SensitivityCubeFactorData (FactorData nested struct)",
        )

    def test_cross_pair_template(self):
        """Assert RiskFactorKeyCrossPair template is available."""
        self.assertTrue(
            hasattr(ORE, "RiskFactorKeyCrossPair"),
            msg="Missing RiskFactorKeyCrossPair template",
        )

    def test_risk_factor_key_set_template(self):
        """Assert RiskFactorKeySet template is available."""
        self.assertTrue(
            hasattr(ORE, "RiskFactorKeySet"),
            msg="Missing RiskFactorKeySet template",
        )

    def test_sensitivity_cube_vector_template(self):
        """Assert SensitivityCubeVector template is available."""
        self.assertTrue(
            hasattr(ORE, "SensitivityCubeVector"),
            msg="Missing SensitivityCubeVector template",
        )


class TestSensitivityCubeFactorData(unittest.TestCase):
    """Tests for the FactorData struct members."""

    def test_factor_data_construction(self):
        """FactorData struct can be instantiated and members accessed."""
        fd = ORE.SensitivityCubeFactorData()
        fd.index = 5
        fd.targetShiftSize = 0.0001
        fd.actualShiftSize = 0.00012
        fd.factorDesc = "DiscountCurve/USD/3Y"

        self.assertEqual(fd.index, 5)
        self.assertAlmostEqual(fd.targetShiftSize, 0.0001)
        self.assertAlmostEqual(fd.actualShiftSize, 0.00012)
        self.assertEqual(fd.factorDesc, "DiscountCurve/USD/3Y")

    def test_factor_data_rfkey(self):
        """FactorData rfkey member is a RiskFactorKey."""
        fd = ORE.SensitivityCubeFactorData()
        key = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "USD", 3)
        fd.rfkey = key
        self.assertEqual(fd.rfkey.name, "USD")
        self.assertEqual(fd.rfkey.index, 3)


class TestCrossPairType(unittest.TestCase):
    """Tests for the crossPair (RiskFactorKeyCrossPair) template."""

    def test_cross_pair_construction(self):
        """Cross pair can be constructed from two RiskFactorKeys."""
        key1 = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "USD", 0)
        key2 = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "EUR", 1)
        pair = ORE.RiskFactorKeyCrossPair(key1, key2)
        self.assertEqual(pair.first.name, "USD")
        self.assertEqual(pair.second.name, "EUR")

    def test_cross_pair_modification(self):
        """Cross pair members are accessible."""
        key1 = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_FXSpot, "EURUSD", 0)
        key2 = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_FXSpot, "GBPUSD", 0)
        pair = ORE.RiskFactorKeyCrossPair(key1, key2)
        self.assertEqual(pair.first.name, "EURUSD")
        self.assertEqual(pair.second.name, "GBPUSD")


class TestRiskFactorKeySet(unittest.TestCase):
    """Tests for the RiskFactorKeySet template."""

    def test_set_construction_and_iteration(self):
        """RiskFactorKeySet supports insertion and iteration."""
        s = ORE.RiskFactorKeySet()
        key1 = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "USD", 0)
        key2 = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "EUR", 0)
        s.insert(key1)
        s.insert(key2)
        self.assertEqual(len(s), 2)

    def test_set_duplicate_rejection(self):
        """RiskFactorKeySet rejects duplicates."""
        s = ORE.RiskFactorKeySet()
        key = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_FXSpot, "EURUSD", 0)
        s.insert(key)
        s.insert(key)
        self.assertEqual(len(s), 1)


class TestSensitivityCubeVector(unittest.TestCase):
    """Tests for the SensitivityCubeVector template."""

    def test_vector_empty(self):
        """SensitivityCubeVector can be created empty."""
        vec = ORE.SensitivityCubeVector()
        self.assertEqual(len(vec), 0)


if __name__ == "__main__":
    unittest.main()

"""
Copyright (C) 2026 Quaternion Risk Management Ltd
All rights reserved.
"""

from ORE import *
import unittest


class QleTask4InstrumentsSmokeTest(unittest.TestCase):
    def test_class_exports(self):
        """Verify Task 4 instrument classes are exported in ORE module."""
        expected = [
            "MultiLegOption",
            "BondTRS",
            "GenericSwaption",
            "RiskParticipationAgreement",
            "ConvertibleBond2",
            "QLECallableBond",
            "BalanceGuaranteedSwap",
            "Ascot",
        ]
        ore_module = __import__("ORE")
        for class_name in expected:
            self.assertTrue(hasattr(ore_module, class_name), class_name)
if __name__ == "__main__":
    unittest.main()

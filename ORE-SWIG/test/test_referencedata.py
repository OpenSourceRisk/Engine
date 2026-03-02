"""
 Copyright (C) 2026 Quaternion Risk Management Ltd
 All rights reserved.
"""

from ORE import *
import unittest


class ReferenceDataBindingsTest(unittest.TestCase):

    def test_reference_datum_subclasses_constructible(self):
        self.assertIsNotNone(BondReferenceDatum("BOND_TEST"))
        self.assertIsNotNone(CreditIndexReferenceDatum("CDX_TEST"))
        self.assertIsNotNone(EquityReferenceDatum("EQ_TEST"))
        self.assertIsNotNone(CurrencyHedgedEquityIndexReferenceDatum("CH_TEST"))

    def test_basic_reference_manager_api_exposed(self):
        self.assertTrue(hasattr(BasicReferenceDataManager, "appendData"))
        self.assertTrue(hasattr(BasicReferenceDataManager, "hasData"))
        self.assertTrue(hasattr(BasicReferenceDataManager, "getData"))


if __name__ == '__main__':
    print('testing ORE reference data bindings')
    unittest.main()

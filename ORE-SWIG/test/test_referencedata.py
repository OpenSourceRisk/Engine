"""Targeted binding and XML round-trip tests for reference data types."""

import unittest

import ORE as ore


class ReferenceDataBindingsTest(unittest.TestCase):
    """Validate newly wrapped reference-data bindings."""

    def _assert_roundtrip(self, instance, factory, expected_fragments=None):
        """Round-trip an XML-serializable object and return the clone."""
        xml = instance.toXMLString()
        self.assertTrue(xml)

        clone = factory()
        clone.fromXMLString(xml)

        roundtripped = clone.toXMLString()
        self.assertTrue(roundtripped)

        for fragment in expected_fragments or []:
            self.assertIn(fragment, roundtripped)

        return clone

    def test_reference_datum_subclasses_constructible(self):
        self.assertIsNotNone(ore.BondReferenceDatum("BOND_TEST"))
        self.assertIsNotNone(ore.BondFutureReferenceDatum("FUT_TEST"))
        self.assertIsNotNone(ore.CreditIndexReferenceDatum("CDX_TEST"))
        self.assertIsNotNone(ore.EquityReferenceDatum("EQ_TEST"))
        self.assertIsNotNone(ore.CurrencyHedgedEquityIndexReferenceDatum("CH_TEST"))
        self.assertIsNotNone(ore.CreditReferenceDatum("CPTY_TEST"))
        self.assertIsNotNone(ore.BondBasketReferenceDatum("BASKET_TEST"))

    def test_bond_reference_roundtrip(self):
        datum = ore.BondReferenceDatum("BOND_TEST")
        datum_copy = self._assert_roundtrip(
            datum,
            ore.BondReferenceDatum,
            ["BOND_TEST", "Bond"],
        )
        self.assertEqual(datum_copy.id(), "BOND_TEST")

    def test_bond_future_credit_and_basket_roundtrip(self):
        future_datum = ore.BondFutureReferenceDatum("FUT_TEST")
        future_copy = self._assert_roundtrip(
            future_datum,
            ore.BondFutureReferenceDatum,
            ["FUT_TEST", "BondFuture"],
        )
        self.assertEqual(future_copy.id(), "FUT_TEST")

        credit_datum = ore.CreditReferenceDatum(
            "CPTY_A",
            "Counterparty A",
            "GroupA",
            "",
            "",
            ore.Date(),
            ore.Date(),
            "Corporate",
            "Spread",
            100.0,
        )
        credit_copy = self._assert_roundtrip(
            credit_datum,
            ore.CreditReferenceDatum,
            ["CPTY_A", "Counterparty A"],
        )
        self.assertEqual(credit_copy.id(), "CPTY_A")

        basket_datum = ore.BondBasketReferenceDatum("BASKET_A")
        basket_copy = self._assert_roundtrip(
            basket_datum,
            ore.BondBasketReferenceDatum,
            ["BASKET_A", "BondBasket"],
        )
        self.assertEqual(basket_copy.id(), "BASKET_A")

    def test_basic_reference_manager_api_exposed(self):
        self.assertTrue(hasattr(ore.BasicReferenceDataManager, "appendData"))
        self.assertTrue(hasattr(ore.BasicReferenceDataManager, "hasData"))
        self.assertTrue(hasattr(ore.BasicReferenceDataManager, "getData"))
        self.assertTrue(hasattr(ore.BasicReferenceDataManager, "buildReferenceDatum"))


if __name__ == '__main__':
    print('testing ORE reference data bindings')
    unittest.main()

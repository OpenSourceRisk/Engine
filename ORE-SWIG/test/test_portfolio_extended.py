"""Targeted binding and XML round-trip tests for portfolio support types."""

import os
import tempfile
import unittest

import ORE as ore


class ExtendedPortfolioBindingsTest(unittest.TestCase):
    """Validate newly wrapped portfolio-side support bindings."""

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

    def test_new_trade_classes_constructible(self):
        self.assertIsNotNone(ore.ForwardBond())
        self.assertIsNotNone(ore.BondOption())
        self.assertIsNotNone(ore.TRS())
        self.assertTrue(hasattr(ore.FxDoubleBarrierOption, "fromXML"))
        self.assertTrue(hasattr(ore.FxEuropeanBarrierOption, "fromXML"))
        self.assertIsNotNone(ore.CommodityDigitalOption())
        self.assertIsNotNone(ore.CommoditySpreadOption())
        self.assertIsNotNone(ore.CommodityAveragePriceOption())
        self.assertIsNotNone(ore.ScriptedTrade())

    def test_new_leg_classes_constructible(self):
        self.assertIsNotNone(ore.CMSSpreadLegData())
        self.assertIsNotNone(ore.DigitalCMSSpreadLegData())
        self.assertIsNotNone(ore.EquityLegData())

    def test_schedule_option_and_trade_action_roundtrip(self):
        schedule_dates = ore.ScheduleDates(
            "TARGET",
            "F",
            "1M",
            ["2026-01-05", "2026-02-05", "2026-03-05"],
        )
        self._assert_roundtrip(
            schedule_dates,
            ore.ScheduleDates,
            ["2026-01-05", "2026-03-05"],
        )

        schedule_derived = ore.ScheduleDerived(
            "BaseSchedule", "TARGET", "F", "2D"
        )
        self._assert_roundtrip(
            schedule_derived,
            ore.ScheduleDerived,
            ["BaseSchedule", "2D"],
        )

        exercise = ore.OptionExerciseData("2026-06-30", "101.25")
        exercise_copy = self._assert_roundtrip(
            exercise,
            ore.OptionExerciseData,
            ["2026-06-30", "101.25"],
        )
        self.assertAlmostEqual(exercise_copy.price(), 101.25)

        payment = ore.OptionPaymentData(["2026-07-02"])
        payment_copy = self._assert_roundtrip(
            payment,
            ore.OptionPaymentData,
            ["2026-07-02"],
        )
        self.assertFalse(payment_copy.rulesBased())

        schedule = ore.ScheduleData(schedule_dates, "ExerciseDates")
        action = ore.TradeAction("Exercise", "Holder", schedule)
        actions = ore.TradeActions()
        actions.addAction(action)
        self._assert_roundtrip(actions, ore.TradeActions, ["Exercise", "Holder"])

    def test_netting_set_and_manager_roundtrip(self):
        netting_set = ore.NettingSetDefinition("CSA_SET")
        netting_copy = self._assert_roundtrip(
            netting_set,
            lambda: ore.NettingSetDefinition("TMP_SET"),
            ["CSA_SET"],
        )
        self.assertEqual(netting_copy.nettingSetId(), "CSA_SET")
        self.assertTrue(hasattr(ore.NettingSetManager, "add"))
        self.assertTrue(hasattr(ore.NettingSetManager, "fromXMLString"))

    def test_scripted_trade_support_and_position_payload_roundtrip(self):
        self._assert_roundtrip(ore.ScriptedTradeScriptData(), ore.ScriptedTradeScriptData)
        event = ore.ScriptedTradeEventData("Fixing", "2026-01-05")
        self._assert_roundtrip(
            event,
            ore.ScriptedTradeEventData,
            ["Fixing", "2026-01-05"],
        )
        self._assert_roundtrip(ore.ScriptLibraryData(), ore.ScriptLibraryData)

        bond_position = ore.BondPositionData(
            10.0,
            [ore.BondUnderlying("ISIN:US912828X703", "ISIN", 1.0)],
        )
        self._assert_roundtrip(
            bond_position,
            ore.BondPositionData,
            ["ISIN:US912828X703"],
        )

        commodity_position = ore.CommodityPositionData(
            5.0,
            [
                ore.CommodityUnderlying(
                    "COMM-NYMEX:CL",
                    1.0,
                    "FutureSettlement",
                    0,
                    0,
                    "NullCalendar",
                )
            ],
        )
        self._assert_roundtrip(
            commodity_position,
            ore.CommodityPositionData,
            ["COMM-NYMEX:CL"],
        )

        equity_underlying = ore.EquityUnderlying("RIC:.SPX")
        equity_position = ore.EquityPositionData(2.0, [equity_underlying])
        self._assert_roundtrip(equity_position, ore.EquityPositionData, ["2"])

        self.assertTrue(hasattr(ore.EquityOptionPositionData, "fromXMLString"))

    def test_portfolio_from_file(self):
        p1 = ore.Portfolio()
        xml = p1.toXMLString()

        fd, path = tempfile.mkstemp(suffix=".xml")
        try:
            os.close(fd)
            with open(path, "w", encoding="utf-8") as handle:
                handle.write(xml)

            p2 = ore.Portfolio()
            p2.fromFile(path)
            self.assertEqual(p2.size(), 0)
        finally:
            if os.path.exists(path):
                os.remove(path)


if __name__ == '__main__':
    print('testing ORE extended portfolio bindings')
    unittest.main()

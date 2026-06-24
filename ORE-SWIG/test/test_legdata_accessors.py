"""
Copyright (C) 2026 AcadiaSoft, Inc.
All rights reserved.

Tests for read accessors on LegData, ScheduleData, and OptionData.

Verifies that data objects constructed from Python can be inspected
via getter methods — covering the acceptance criteria for the SWIG
trade representation ticket.
"""

from ORE import (
    ScheduleRules,
    ScheduleDates,
    ScheduleData,
    FixedLegData,
    FloatingLegData,
    LegData,
    OptionData,
    LegType_Fixed,
    LegType_Floating,
)
import unittest


class ScheduleRulesAccessorTest(unittest.TestCase):
    """Tests for ScheduleRules getter methods."""

    def setUp(self) -> None:
        """Construct a standard ScheduleRules object for inspection."""
        self.rules = ScheduleRules(
            "2024-01-15",
            "2029-01-15",
            "6M",
            "TARGET",
            "MF",
            "MF",
            "Forward",
        )

    def test_start_date(self) -> None:
        """startDate() returns the value passed at construction."""
        self.assertEqual(self.rules.startDate(), "2024-01-15")

    def test_end_date(self) -> None:
        """endDate() returns the value passed at construction."""
        self.assertEqual(self.rules.endDate(), "2029-01-15")

    def test_tenor(self) -> None:
        """tenor() returns the value passed at construction."""
        self.assertEqual(self.rules.tenor(), "6M")

    def test_calendar(self) -> None:
        """calendar() returns the value passed at construction."""
        self.assertEqual(self.rules.calendar(), "TARGET")

    def test_convention(self) -> None:
        """convention() returns the value passed at construction."""
        self.assertEqual(self.rules.convention(), "MF")

    def test_rule(self) -> None:
        """rule() returns the value passed at construction."""
        self.assertEqual(self.rules.rule(), "Forward")

    def test_has_data(self) -> None:
        """hasData() returns True when startDate and tenor are set."""
        self.assertTrue(self.rules.hasData())

    def test_empty_rules_has_no_data(self) -> None:
        """hasData() returns False for a default-constructed ScheduleRules."""
        empty = ScheduleRules()
        self.assertFalse(empty.hasData())


class ScheduleDataAccessorTest(unittest.TestCase):
    """Tests for ScheduleData and ScheduleDates getter methods."""

    def setUp(self) -> None:
        """Construct ScheduleData from rules and from explicit dates."""
        rules = ScheduleRules(
            "2024-01-15", "2029-01-15", "6M", "TARGET", "MF", "MF", "Forward"
        )
        self.schedule_from_rules = ScheduleData(rules)

        dates_obj = ScheduleDates(
            "TARGET", "MF", "6M", ["2024-01-15", "2024-07-15", "2025-01-15"]
        )
        self.schedule_from_dates = ScheduleData(dates_obj)

    def test_rules_accessor_returns_correct_count(self) -> None:
        """rules() returns a vector with one ScheduleRules entry."""
        self.assertEqual(len(self.schedule_from_rules.rules()), 1)

    def test_rules_start_date_round_trips(self) -> None:
        """The startDate on the retrieved rule matches what was set."""
        rule = self.schedule_from_rules.rules()[0]
        self.assertEqual(rule.startDate(), "2024-01-15")

    def test_dates_accessor_returns_correct_count(self) -> None:
        """dates() returns a vector with one ScheduleDates entry."""
        self.assertEqual(len(self.schedule_from_dates.dates()), 1)

    def test_dates_entries_round_trip(self) -> None:
        """The date strings on the retrieved ScheduleDates match what was set."""
        dates_entry = self.schedule_from_dates.dates()[0]
        retrieved = list(dates_entry.dates())
        self.assertEqual(retrieved, ["2024-01-15", "2024-07-15", "2025-01-15"])

    def test_has_data_true(self) -> None:
        """hasData() is True for a ScheduleData with rules."""
        self.assertTrue(self.schedule_from_rules.hasData())

    def test_empty_schedule_has_no_data(self) -> None:
        """hasData() is False for a default-constructed ScheduleData."""
        self.assertFalse(ScheduleData().hasData())


class LegDataAccessorTest(unittest.TestCase):
    """Tests for LegData getter methods.

    Covers acceptance criteria: isPayer(), currency(), notionals(),
    schedule(), dayCounter(), legType(), and concreteLegData().
    """

    def setUp(self) -> None:
        """Construct fixed and floating LegData objects for inspection."""
        rules = ScheduleRules(
            "2024-01-15", "2029-01-15", "6M", "TARGET", "MF", "MF", "Forward"
        )
        schedule = ScheduleData(rules)
        notionals = [10_000_000.0]

        self.fixed_leg = LegData(
            FixedLegData([0.03]),
            True,   # isPayer
            "EUR",
            schedule,
            "30/360",
            notionals,
        )

        self.float_leg = LegData(
            FloatingLegData("EUR-EURIBOR-6M", 2, False, [0.001]),
            False,  # isPayer
            "EUR",
            schedule,
            "ACT/360",
            notionals,
        )

    def test_is_payer_fixed(self) -> None:
        """isPayer() returns True for the payer leg."""
        self.assertTrue(self.fixed_leg.isPayer())

    def test_is_payer_float(self) -> None:
        """isPayer() returns False for the receiver leg."""
        self.assertFalse(self.float_leg.isPayer())

    def test_currency(self) -> None:
        """currency() returns the currency passed at construction."""
        self.assertEqual(self.fixed_leg.currency(), "EUR")

    def test_notionals(self) -> None:
        """notionals() returns the notional vector passed at construction."""
        notionals = list(self.fixed_leg.notionals())
        self.assertEqual(len(notionals), 1)
        self.assertAlmostEqual(notionals[0], 10_000_000.0)

    def test_day_counter(self) -> None:
        """dayCounter() returns the day count convention string."""
        self.assertEqual(self.fixed_leg.dayCounter(), "30/360")

    def test_leg_type_fixed(self) -> None:
        """legType() returns LegType_Fixed for a FixedLegData leg."""
        self.assertEqual(self.fixed_leg.legType(), LegType_Fixed)

    def test_leg_type_floating(self) -> None:
        """legType() returns LegType_Floating for a FloatingLegData leg."""
        self.assertEqual(self.float_leg.legType(), LegType_Floating)

    def test_concrete_leg_data_not_none(self) -> None:
        """concreteLegData() returns a non-null shared_ptr."""
        self.assertIsNotNone(self.fixed_leg.concreteLegData())

    def test_schedule_rules_accessible(self) -> None:
        """schedule() returns a ScheduleData with one rule entry."""
        sched = self.fixed_leg.schedule()
        self.assertTrue(sched.hasData())
        self.assertEqual(len(sched.rules()), 1)

    def test_round_trip_currency_matches_float(self) -> None:
        """currency() is consistent across both legs."""
        self.assertEqual(self.fixed_leg.currency(), self.float_leg.currency())


class OptionDataAccessorTest(unittest.TestCase):
    """Tests for OptionData getter methods.

    Covers acceptance criteria: longShort(), callPut(), style(),
    exerciseDates(), and settlement().
    """

    def setUp(self) -> None:
        """Construct a European call OptionData for inspection."""
        self.option = OptionData(
            "Long",
            "Call",
            "European",
            True,
            ["2029-01-15"],
            "Cash",
        )

        self.bermudan_put = OptionData(
            "Short",
            "Put",
            "Bermudan",
            False,
            ["2026-01-15", "2027-01-15", "2028-01-15"],
            "Physical",
        )

    def test_long_short(self) -> None:
        """longShort() returns the long/short direction."""
        self.assertEqual(self.option.longShort(), "Long")

    def test_call_put(self) -> None:
        """callPut() returns the call/put type."""
        self.assertEqual(self.option.callPut(), "Call")

    def test_style(self) -> None:
        """style() returns the exercise style string."""
        self.assertEqual(self.option.style(), "European")

    def test_payoff_at_expiry(self) -> None:
        """payoffAtExpiry() returns True when set at construction."""
        self.assertTrue(self.option.payoffAtExpiry())

    def test_exercise_dates_single(self) -> None:
        """exerciseDates() returns the single exercise date."""
        dates = list(self.option.exerciseDates())
        self.assertEqual(dates, ["2029-01-15"])

    def test_settlement(self) -> None:
        """settlement() returns the settlement type string."""
        self.assertEqual(self.option.settlement(), "Cash")

    def test_bermudan_long_short(self) -> None:
        """longShort() returns Short for the put option."""
        self.assertEqual(self.bermudan_put.longShort(), "Short")

    def test_bermudan_exercise_dates(self) -> None:
        """exerciseDates() returns all three Bermudan exercise dates."""
        dates = list(self.bermudan_put.exerciseDates())
        self.assertEqual(
            dates, ["2026-01-15", "2027-01-15", "2028-01-15"]
        )

    def test_bermudan_settlement(self) -> None:
        """settlement() returns Physical for the Bermudan put."""
        self.assertEqual(self.bermudan_put.settlement(), "Physical")

    def test_bermudan_style(self) -> None:
        """style() returns Bermudan for the second option."""
        self.assertEqual(self.bermudan_put.style(), "Bermudan")


if __name__ == "__main__":
    unittest.main()

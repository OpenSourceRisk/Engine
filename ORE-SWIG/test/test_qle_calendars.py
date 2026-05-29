"""
Copyright (C) 2026 Quaternion Risk Management Ltd
All rights reserved.

Focused Python wrapper tests for the seven QuantExt calendar classes added
in ACADIAQPR-14062: AmendedCalendar, Austria, France, Israel, Mauritius,
Switzerland, and UnitedArabEmirates.
"""

import unittest

from ORE import (
    AmendedCalendar,
    Date,
    Friday,
    January,
    Mauritius,
    October,
    QLEAustria,
    QLEFrance,
    QLEIsrael,
    QLESwitzerland,
    Saturday,
    Sunday,
    TARGET,
    UnitedArabEmirates,
    WeekendsOnly,
)
import ORE


class AmendedCalendarTest(unittest.TestCase):
    """Tests for QuantExt::AmendedCalendar wrapper."""

    def test_construct_from_weekends_only(self) -> None:
        """Verify construction from WeekendsOnly base calendar."""
        base = WeekendsOnly()
        cal = AmendedCalendar(base, "MyCustomCal")
        self.assertIsNotNone(cal)

    def test_custom_name_is_preserved(self) -> None:
        """Verify the name passed at construction is returned by name()."""
        base = TARGET()
        cal = AmendedCalendar(base, "AmendedTARGET")
        self.assertEqual(cal.name(), "AmendedTARGET")

    def test_add_holiday_affects_instance(self) -> None:
        """Verify addHoliday() marks a previously open day as a holiday."""
        base = WeekendsOnly()
        cal = AmendedCalendar(base, "TestCal")
        # 3 January 2024 is a Wednesday — open in WeekendsOnly
        wednesday = Date(3, January, 2024)
        self.assertTrue(cal.isBusinessDay(wednesday))
        cal.addHoliday(wednesday)
        self.assertFalse(cal.isBusinessDay(wednesday))


class AustriaTest(unittest.TestCase):
    """Tests for QuantExt::Austria calendar wrapper (exposed as QLEAustria)."""

    def test_default_construction(self) -> None:
        """Verify default constructor succeeds."""
        cal = QLEAustria()
        self.assertIsNotNone(cal)

    def test_construction_with_settlement_enum(self) -> None:
        """Verify construction with QLEAustria.Settlement enum."""
        cal = QLEAustria(QLEAustria.Settlement)
        self.assertIsNotNone(cal)

    def test_new_years_day_is_holiday(self) -> None:
        """January 1st should be a holiday."""
        cal = QLEAustria()
        self.assertFalse(cal.isBusinessDay(Date(1, January, 2020)))

    def test_epiphany_is_holiday(self) -> None:
        """January 6th (Epiphany) is an Austrian public holiday."""
        cal = QLEAustria()
        self.assertFalse(cal.isBusinessDay(Date(6, January, 2020)))

    def test_national_day_is_holiday(self) -> None:
        """October 26th (National Day) is an Austrian public holiday."""
        cal = QLEAustria()
        self.assertFalse(cal.isBusinessDay(Date(26, October, 2020)))


class FranceTest(unittest.TestCase):
    """Tests for QuantExt::France calendar wrapper (exposed as QLEFrance)."""

    def test_default_construction(self) -> None:
        """Verify default constructor succeeds."""
        cal = QLEFrance()
        self.assertIsNotNone(cal)

    def test_construction_with_settlement_enum(self) -> None:
        """Verify construction with QLEFrance.Settlement enum."""
        cal = QLEFrance(QLEFrance.Settlement)
        self.assertIsNotNone(cal)

    def test_2018_holiday_list(self) -> None:
        """Mirror the holiday list checked in qle_calendars.cpp testFrenchCalendar."""
        from ORE import (March, April, May, August, November, December)
        cal = QLEFrance()
        expected = [
            Date(1, January, 2018),
            Date(30, March, 2018),     # Good Friday
            Date(2, April, 2018),      # Easter Monday
            Date(1, May, 2018),        # Labour Day
            Date(8, May, 2018),        # Victory in Europe Day
            Date(10, May, 2018),       # Ascension Day
            Date(21, May, 2018),       # Whit Monday
            Date(15, August, 2018),    # Assumption of Mary
            Date(1, November, 2018),   # All Saints' Day
            Date(25, December, 2018),  # Christmas Day
            Date(26, December, 2018),  # St. Stephen's Day
        ]
        hols = cal.holidayList(
            Date(1, January, 2018), Date(31, December, 2018)
        )
        self.assertEqual(len(list(hols)), len(expected))
        for expected_date, actual_date in zip(expected, hols):
            self.assertEqual(expected_date, actual_date)


class IsraelTest(unittest.TestCase):
    """Tests for QuantExt::Israel calendar wrapper (exposed as QLEIsrael)."""

    def test_enum_visibility(self) -> None:
        """Verify MarketExt enum values are accessible on the class."""
        self.assertTrue(hasattr(QLEIsrael, "Telbor"))
        self.assertTrue(hasattr(QLEIsrael, "TASE"))
        self.assertTrue(hasattr(QLEIsrael, "Settlement"))

    def test_default_is_telbor(self) -> None:
        """Default constructor should produce a Telbor calendar."""
        cal = QLEIsrael()
        # Telbor: Saturday and Sunday are weekends; Friday is not
        self.assertFalse(cal.isWeekend(Friday))
        self.assertTrue(cal.isWeekend(Saturday))
        self.assertTrue(cal.isWeekend(Sunday))

    def test_telbor_weekends(self) -> None:
        """Telbor calendar uses Sat/Sun weekends (Western convention)."""
        cal = QLEIsrael(QLEIsrael.Telbor)
        self.assertFalse(cal.isWeekend(Friday))
        self.assertTrue(cal.isWeekend(Saturday))
        self.assertTrue(cal.isWeekend(Sunday))

    def test_settlement_weekends(self) -> None:
        """Settlement calendar uses Fri/Sat weekends (Israeli convention)."""
        cal = QLEIsrael(QLEIsrael.Settlement)
        self.assertTrue(cal.isWeekend(Friday))
        self.assertTrue(cal.isWeekend(Saturday))
        self.assertFalse(cal.isWeekend(Sunday))


class MauritiusTest(unittest.TestCase):
    """Tests for QuantExt::Mauritius calendar wrapper."""

    def test_default_construction(self) -> None:
        """Verify default constructor succeeds."""
        cal = Mauritius()
        self.assertIsNotNone(cal)

    def test_sem_enum_visibility(self) -> None:
        """Verify the SEM market enum is accessible on the class."""
        self.assertTrue(hasattr(Mauritius, "SEM"))

    def test_construction_with_sem_enum(self) -> None:
        """Verify construction with Mauritius.SEM enum."""
        cal = Mauritius(Mauritius.SEM)
        self.assertIsNotNone(cal)

    def test_fixed_holidays(self) -> None:
        """Fixed annual holidays should be closed regardless of year."""
        from ORE import March, May, November, December
        cal = Mauritius()
        self.assertFalse(
            cal.isBusinessDay(Date(12, March, 2022))   # Independence Day
        )
        self.assertFalse(
            cal.isBusinessDay(Date(25, December, 2022))  # Christmas
        )

    def test_2022_explicit_holidays(self) -> None:
        """Source encodes explicit non-Gregorian holidays for 2022."""
        from ORE import March, May, August, September
        cal = Mauritius()
        self.assertFalse(
            cal.isBusinessDay(Date(3, January, 2022))   # New year holiday
        )
        self.assertFalse(
            cal.isBusinessDay(Date(18, January, 2022))  # Thaipoosam Cavadee
        )
        self.assertFalse(
            cal.isBusinessDay(Date(24, October, 2022))  # Divali
        )

    def test_2023_explicit_holidays(self) -> None:
        """Source encodes explicit non-Gregorian holidays for 2023."""
        from ORE import March
        cal = Mauritius()
        self.assertFalse(
            cal.isBusinessDay(Date(2, January, 2023))   # New year holiday
        )
        self.assertFalse(
            cal.isBusinessDay(Date(3, January, 2023))   # New year holiday
        )


class SwitzerlandTest(unittest.TestCase):
    """Tests for QuantExt::Switzerland wrapper (exposed as QLESwitzerland, adds SIX market)."""

    def test_default_construction(self) -> None:
        """Verify default constructor succeeds."""
        cal = QLESwitzerland()
        self.assertIsNotNone(cal)

    def test_enum_visibility(self) -> None:
        """Verify Settlement and SIX enum values are accessible."""
        self.assertTrue(hasattr(QLESwitzerland, "Settlement"))
        self.assertTrue(hasattr(QLESwitzerland, "SIX"))

    def test_six_closes_on_christmas_eve(self) -> None:
        """SIX closes on Dec 24; Settlement does not (when not a weekend)."""
        from ORE import December
        # 24 December 2021 is a Friday
        six = QLESwitzerland(QLESwitzerland.SIX)
        settlement = QLESwitzerland(QLESwitzerland.Settlement)
        self.assertFalse(six.isBusinessDay(Date(24, December, 2021)))
        self.assertTrue(settlement.isBusinessDay(Date(24, December, 2021)))

    def test_six_closes_on_new_years_eve(self) -> None:
        """SIX closes on Dec 31; Settlement does not (when not a weekend)."""
        from ORE import December
        # 31 December 2021 is a Friday
        six = QLESwitzerland(QLESwitzerland.SIX)
        settlement = QLESwitzerland(QLESwitzerland.Settlement)
        self.assertFalse(six.isBusinessDay(Date(31, December, 2021)))
        self.assertTrue(settlement.isBusinessDay(Date(31, December, 2021)))


class UnitedArabEmiratesTest(unittest.TestCase):
    """Tests for QuantExt::UnitedArabEmirates calendar wrapper."""

    def test_construction(self) -> None:
        """Verify default constructor succeeds."""
        cal = UnitedArabEmirates()
        self.assertIsNotNone(cal)

    def test_pre_2022_friday_saturday_weekend(self) -> None:
        """Before 2022 UAE uses Fri/Sat weekends."""
        from ORE import February, Thursday
        cal = UnitedArabEmirates()
        # February 2021: Thu=4, Fri=5, Sat=6, Sun=7
        self.assertTrue(cal.isBusinessDay(Date(4, February, 2021)))   # Thu
        self.assertFalse(cal.isBusinessDay(Date(5, February, 2021)))  # Fri
        self.assertFalse(cal.isBusinessDay(Date(6, February, 2021)))  # Sat
        self.assertTrue(cal.isBusinessDay(Date(7, February, 2021)))   # Sun

    def test_post_2022_saturday_sunday_weekend(self) -> None:
        """From 2022 UAE switched to Sat/Sun weekends."""
        from ORE import February
        cal = UnitedArabEmirates()
        # February 2022: Fri=4, Sat=5, Sun=6, Mon=7
        self.assertTrue(cal.isBusinessDay(Date(4, February, 2022)))   # Fri
        self.assertFalse(cal.isBusinessDay(Date(5, February, 2022)))  # Sat
        self.assertFalse(cal.isBusinessDay(Date(6, February, 2022)))  # Sun
        self.assertTrue(cal.isBusinessDay(Date(7, February, 2022)))   # Mon


class QleCalendarExportSmokeTest(unittest.TestCase):
    """Verify all seven new calendar symbols are exported in the ORE module."""

    def test_all_new_calendar_classes_exported(self) -> None:
        """Each new calendar class must be accessible as a top-level ORE symbol."""
        new_calendars = [
            "AmendedCalendar",
            "QLEAustria",
            "QLEFrance",
            "QLEIsrael",
            "Mauritius",
            "QLESwitzerland",
            "UnitedArabEmirates",
        ]
        for name in new_calendars:
            self.assertTrue(
                hasattr(ORE, name),
                msg=f"Missing ORE module symbol: {name}",
            )


if __name__ == "__main__":
    unittest.main()

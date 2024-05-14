/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2023 Skandinaviska Enskilda Banken AB (publ)
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <boost/test/unit_test.hpp>
#include <iostream>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/currencies/america.hpp>
#include <ql/math/comparison.hpp>
#include <ql/time/calendars/austria.hpp>
#include <ql/time/calendars/chile.hpp>
#include <ql/time/calendars/france.hpp>
#include <ql/time/calendars/jointcalendar.hpp>
#include <ql/time/calendars/thailand.hpp>
#include <ql/time/daycounters/all.hpp>
#include <qle/calendars/colombia.hpp>
#include <qle/calendars/israel.hpp>
#include <qle/calendars/malaysia.hpp>
#include <qle/calendars/netherlands.hpp>
#include <qle/calendars/peru.hpp>
#include <qle/calendars/philippines.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;

using ore::data::CommodityForwardQuote;
using ore::data::CommoditySpotQuote;
using ore::data::FXOptionQuote;
using ore::data::MarketDatum;
using ore::data::parseMarketDatum;

namespace {

struct test_daycounter_data {
    const char* str;
    DayCounter dc;
};

static struct test_daycounter_data daycounter_data[] = {
    {"A360", Actual360()},
    {"Actual/360", Actual360()},
    {"ACT/360", Actual360()},
    {"A365", Actual365Fixed()},
    {"A365F", Actual365Fixed()},
    {"Actual/365 (Fixed)", Actual365Fixed()},
    {"ACT/365", Actual365Fixed()},
    {"T360", Thirty360(Thirty360::USA)},
    {"30/360", Thirty360(Thirty360::USA)},
    {"30/360 (Bond Basis)", Thirty360(Thirty360::BondBasis)},
    {"ACT/nACT", Thirty360(Thirty360::USA)},
    {"30E/360 (Eurobond Basis)", Thirty360(Thirty360::European)},
    {"30E/360", Thirty360(Thirty360::European)},
    {"30/360 (Italian)", Thirty360(Thirty360::Italian)},
    {"ActActISDA", ActualActual(ActualActual::ISDA)},
    {"Actual/Actual (ISDA)", ActualActual(ActualActual::ISDA)},
    {"ACT/ACT", ActualActual(ActualActual::ISDA)},
    {"ACT29", ActualActual(ActualActual::AFB)},
    {"ACT", ActualActual(ActualActual::ISDA)},
    {"ActActISMA", ActualActual(ActualActual::ISMA)},
    {"Actual/Actual (ISMA)", ActualActual(ActualActual::ISMA)},
    {"ActActAFB", ActualActual(ActualActual::AFB)},
    {"Actual/Actual (AFB)", ActualActual(ActualActual::AFB)},
    {"1/1", OneDayCounter()},
    {"BUS/252", Business252()},
    {"Business/252", Business252()},
    {"Actual/365 (No Leap)", Actual365Fixed(Actual365Fixed::NoLeap)},
    {"Act/365 (NL)", Actual365Fixed(Actual365Fixed::NoLeap)},
    {"NL/365", Actual365Fixed(Actual365Fixed::NoLeap)},
    {"Actual/365 (JGB)", Actual365Fixed(Actual365Fixed::NoLeap)},
    {"Actual/364", Actual364()}};

struct test_freq_data {
    const char* str;
    Frequency freq;
};

static struct test_freq_data freq_data[] = {{"Z", Once},
                                            {"Once", Once},
                                            {"A", Annual},
                                            {"Annual", Annual},
                                            {"S", Semiannual},
                                            {"Semiannual", Semiannual},
                                            {"Q", Quarterly},
                                            {"Quarterly", Quarterly},
                                            {"B", Bimonthly},
                                            {"Bimonthly", Bimonthly},
                                            {"M", Monthly},
                                            {"Monthly", Monthly},
                                            {"L", EveryFourthWeek},
                                            {"Lunarmonth", EveryFourthWeek},
                                            {"W", Weekly},
                                            {"Weekly", Weekly},
                                            {"D", Daily},
                                            {"Daily", Daily}};

struct test_comp_data {
    const char* str;
    Compounding comp;
};

static struct test_comp_data comp_data[] = {
    {"Simple", Simple},
    {"Compounded", Compounded},
    {"Continuous", Continuous},
    {"SimpleThenCompounded", SimpleThenCompounded},
};

void checkStrikeParser(const std::string& s, const ore::data::Strike::Type expectedType, const Real expectedValue) {
    if (ore::data::parseStrike(s).type != expectedType) {
        BOOST_FAIL("unexpected strike type parsed from input string " << s);
    }
    if (!close_enough(ore::data::parseStrike(s).value, expectedValue)) {
        BOOST_FAIL("unexpected strike value parsed from input string " << s);
    }
    return;
}

void checkCalendars(const std::set<Date>& expectedHolidays, const std::vector<Date>& testHolidays) {

    for (auto eh : expectedHolidays) {
        if (std::find(testHolidays.begin(), testHolidays.end(), eh) == testHolidays.end())
            BOOST_FAIL("expected holiday was " << eh << " not found ");
    }
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ParserTests)

BOOST_AUTO_TEST_CASE(testDayCounterParsing) {

    BOOST_TEST_MESSAGE("Testing day counter parsing...");

    Size len = sizeof(daycounter_data) / sizeof(daycounter_data[0]);
    for (Size i = 0; i < len; ++i) {
        string str(daycounter_data[i].str);
        QuantLib::DayCounter d;

        try {
            d = ore::data::parseDayCounter(str);
        } catch (...) {
            BOOST_FAIL("Day Counter Parser failed to parse " << str);
        }
        if (d.empty() || d != daycounter_data[i].dc) {
            BOOST_FAIL("Day Counter Parser(" << str << ") returned day counter " << d << " expected "
                                             << daycounter_data[i].dc);

            BOOST_TEST_MESSAGE("Parsed \"" << str << "\" and got " << d);
        }
    }
}

BOOST_AUTO_TEST_CASE(testFrequencyParsing) {

    BOOST_TEST_MESSAGE("Testing frequency parsing...");

    Size len = sizeof(freq_data) / sizeof(freq_data[0]);
    for (Size i = 0; i < len; ++i) {
        string str(freq_data[i].str);

        try {
            QuantLib::Frequency f = ore::data::parseFrequency(str);
            if (f) {
                if (f != freq_data[i].freq)
                    BOOST_FAIL("Frequency Parser(" << str << ") returned frequency " << f << " expected "
                                                   << freq_data[i].freq);
                BOOST_TEST_MESSAGE("Parsed \"" << str << "\" and got " << f);
            }
        } catch (...) {
            BOOST_FAIL("Frequency Parser failed to parse " << str);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCompoundingParsing) {

    BOOST_TEST_MESSAGE("Testing Compounding parsing...");

    Size len = sizeof(comp_data) / sizeof(comp_data[0]);
    for (Size i = 0; i < len; ++i) {
        string str(comp_data[i].str);

        try {
            QuantLib::Compounding c = ore::data::parseCompounding(str);
            if (c) {
                if (c != comp_data[i].comp)
                    BOOST_FAIL("Compounding Parser(" << str << ") returned Compounding " << c << " expected "
                                                     << comp_data[i].comp);
                BOOST_TEST_MESSAGE("Parsed \"" << str << "\" and got " << c);
            }
        } catch (...) {
            BOOST_FAIL("Compounding Parser failed to parse " << str);
        }
    }
}

BOOST_AUTO_TEST_CASE(testStrikeParsing) {

    BOOST_TEST_MESSAGE("Testing Strike parsing...");

    checkStrikeParser("ATM", ore::data::Strike::Type::ATM, 0.0);
    checkStrikeParser("atm", ore::data::Strike::Type::ATM, 0.0);
    checkStrikeParser("ATMF", ore::data::Strike::Type::ATMF, 0.0);
    checkStrikeParser("atmf", ore::data::Strike::Type::ATMF, 0.0);
    checkStrikeParser("ATM+0", ore::data::Strike::Type::ATM_Offset, 0.0);
    checkStrikeParser("ATM-1", ore::data::Strike::Type::ATM_Offset, -1.0);
    checkStrikeParser("ATM+1", ore::data::Strike::Type::ATM_Offset, 1.0);
    checkStrikeParser("ATM-0.01", ore::data::Strike::Type::ATM_Offset, -0.01);
    checkStrikeParser("ATM+0.01", ore::data::Strike::Type::ATM_Offset, 0.01);
    checkStrikeParser("atm+0", ore::data::Strike::Type::ATM_Offset, 0.0);
    checkStrikeParser("atm-1", ore::data::Strike::Type::ATM_Offset, -1.0);
    checkStrikeParser("atm+1", ore::data::Strike::Type::ATM_Offset, 1.0);
    checkStrikeParser("atm-0.01", ore::data::Strike::Type::ATM_Offset, -0.01);
    checkStrikeParser("atm+0.01", ore::data::Strike::Type::ATM_Offset, 0.01);
    checkStrikeParser("1", ore::data::Strike::Type::Absolute, 1.0);
    checkStrikeParser("0.01", ore::data::Strike::Type::Absolute, 0.01);
    checkStrikeParser("+0.01", ore::data::Strike::Type::Absolute, 0.01);
    checkStrikeParser("-0.01", ore::data::Strike::Type::Absolute, -0.01);
    checkStrikeParser("10d", ore::data::Strike::Type::Delta, 10.0);
    checkStrikeParser("10.0d", ore::data::Strike::Type::Delta, 10.0);
    checkStrikeParser("+10d", ore::data::Strike::Type::Delta, 10.0);
    checkStrikeParser("+10.0d", ore::data::Strike::Type::Delta, 10.0);
    checkStrikeParser("-25d", ore::data::Strike::Type::Delta, -25.0);
    checkStrikeParser("-25.0d", ore::data::Strike::Type::Delta, -25.0);
    checkStrikeParser("10D", ore::data::Strike::Type::Delta, 10.0);
    checkStrikeParser("10.0D", ore::data::Strike::Type::Delta, 10.0);
    checkStrikeParser("+10D", ore::data::Strike::Type::Delta, 10.0);
    checkStrikeParser("+10.0D", ore::data::Strike::Type::Delta, 10.0);
    checkStrikeParser("-25D", ore::data::Strike::Type::Delta, -25.0);
    checkStrikeParser("-25.0D", ore::data::Strike::Type::Delta, -25.0);
    checkStrikeParser("10C", ore::data::Strike::Type::DeltaCall, 10.0);
    checkStrikeParser("10c", ore::data::Strike::Type::DeltaCall, 10.0);
    checkStrikeParser("20P", ore::data::Strike::Type::DeltaPut, 20.0);
    checkStrikeParser("20p", ore::data::Strike::Type::DeltaPut, 20.0);
    checkStrikeParser("25BF", ore::data::Strike::Type::BF, 25.0);
    checkStrikeParser("25bf", ore::data::Strike::Type::BF, 25.0);
    checkStrikeParser("25RR", ore::data::Strike::Type::RR, 25.0);
    checkStrikeParser("25rr", ore::data::Strike::Type::RR, 25.0);
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_CASE(testDatePeriodParsing) {

    BOOST_TEST_MESSAGE("Testing Date and Period parsing...");

    BOOST_CHECK_EQUAL(ore::data::parseDate("20170605"), Date(5, Jun, 2017));
    //
    BOOST_CHECK_EQUAL(ore::data::parseDate("2017-06-05"), Date(5, Jun, 2017));
    BOOST_CHECK_EQUAL(ore::data::parseDate("2017/06/05"), Date(5, Jun, 2017));
    BOOST_CHECK_EQUAL(ore::data::parseDate("2017.06.05"), Date(5, Jun, 2017));
    //
    BOOST_CHECK_EQUAL(ore::data::parseDate("05-06-2017"), Date(5, Jun, 2017));
    BOOST_CHECK_EQUAL(ore::data::parseDate("05/06/2017"), Date(5, Jun, 2017));
    BOOST_CHECK_EQUAL(ore::data::parseDate("05.06.2017"), Date(5, Jun, 2017));
    //
    BOOST_CHECK_EQUAL(ore::data::parseDate("05-06-17"), Date(5, Jun, 2017));
    BOOST_CHECK_EQUAL(ore::data::parseDate("05/06/17"), Date(5, Jun, 2017));
    BOOST_CHECK_EQUAL(ore::data::parseDate("05.06.17"), Date(5, Jun, 2017));
    //
    BOOST_CHECK_THROW(ore::data::parseDate("1Y"), QuantLib::Error);
    BOOST_CHECK_THROW(ore::data::parseDate("05-06-1Y"), QuantLib::Error);
    BOOST_CHECK_THROW(ore::data::parseDate("X5-06-17"), QuantLib::Error);
    BOOST_CHECK_THROW(ore::data::parseDate("2017-06-05-"), QuantLib::Error);
    BOOST_CHECK_THROW(ore::data::parseDate("-2017-06-05"), QuantLib::Error);
    BOOST_CHECK_THROW(ore::data::parseDate("xx17-06-05"), QuantLib::Error);

    BOOST_CHECK_EQUAL(ore::data::parsePeriod("3Y"), 3 * Years);
    BOOST_CHECK_EQUAL(ore::data::parsePeriod("3y"), 3 * Years);
    BOOST_CHECK_EQUAL(ore::data::parsePeriod("3M"), 3 * Months);
    BOOST_CHECK_EQUAL(ore::data::parsePeriod("3m"), 3 * Months);
    BOOST_CHECK_EQUAL(ore::data::parsePeriod("3W"), 3 * Weeks);
    BOOST_CHECK_EQUAL(ore::data::parsePeriod("3w"), 3 * Weeks);
    BOOST_CHECK_EQUAL(ore::data::parsePeriod("3D"), 3 * Days);
    BOOST_CHECK_EQUAL(ore::data::parsePeriod("3d"), 3 * Days);
    //
    BOOST_CHECK_EQUAL(ore::data::parsePeriod("1Y6M"), 1 * Years + 6 * Months);
    BOOST_CHECK_EQUAL(ore::data::parsePeriod("6M0W"), 6 * Months + 0 * Weeks);
    BOOST_CHECK_EQUAL(ore::data::parsePeriod("6M0D"), 6 * Months + 0 * Days);
    //
    BOOST_CHECK_THROW(ore::data::parsePeriod("20170605"), QuantLib::Error);
    BOOST_CHECK_THROW(ore::data::parsePeriod("3X"), QuantLib::Error);
    BOOST_CHECK_THROW(ore::data::parsePeriod("xY"), QuantLib::Error);
    // QL moved to std::stoi in its period and date parsers
    // BOOST_CHECK_THROW(ore::data::parsePeriod(".3M"), QuantLib::Error);
    BOOST_CHECK_THROW(ore::data::parsePeriod("3M."), QuantLib::Error);

    Date d;
    Period p;
    bool isDate;

    ore::data::parseDateOrPeriod("20170605", d, p, isDate);
    BOOST_CHECK(isDate && d == Date(5, Jun, 2017));
    ore::data::parseDateOrPeriod("3Y", d, p, isDate);
    BOOST_CHECK(!isDate && p == 3 * Years);
    ore::data::parseDateOrPeriod("3M", d, p, isDate);
    BOOST_CHECK(!isDate && p == 3 * Months);
    ore::data::parseDateOrPeriod("3W", d, p, isDate);
    BOOST_CHECK(!isDate && p == 3 * Weeks);
    ore::data::parseDateOrPeriod("3D", d, p, isDate);
    BOOST_CHECK(!isDate && p == 3 * Days);
    ore::data::parseDateOrPeriod("1Y6M", d, p, isDate);
    BOOST_CHECK(!isDate && p == 1 * Years + 6 * Months);
    ore::data::parseDateOrPeriod("20170605D", d, p, isDate);
    BOOST_CHECK(!isDate && p == 20170605 * Days);
    //
    BOOST_CHECK_THROW(ore::data::parseDateOrPeriod("5Y2017", d, p, isDate), QuantLib::Error);
    // QL moved to std::stoi in its period and date parsers
    // BOOST_CHECK_THROW(ore::data::parseDateOrPeriod("2017-06-05D", d, p, isDate), QuantLib::Error);
    // BOOST_CHECK_THROW(ore::data::parseDateOrPeriod(".3M", d, p, isDate), QuantLib::Error);
    BOOST_CHECK_THROW(ore::data::parseDateOrPeriod("3M.", d, p, isDate), QuantLib::Error);
    BOOST_CHECK_THROW(ore::data::parseDateOrPeriod("xx17-06-05", d, p, isDate), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testMarketDatumParsing) {

    BOOST_TEST_MESSAGE("Testing market datum parsing...");

    BOOST_TEST_MESSAGE("Testing cap/floor market datum parsing...");

    { // test capfloor normal vol ATM
        Date d(1, Jan, 1990);
        Real value = 0.01;

        string input = "CAPFLOOR/RATE_NVOL/USD/5Y/3M/0/0/0";

        QuantLib::ext::shared_ptr<ore::data::MarketDatum> datum = ore::data::parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->asofDate() == d);
        BOOST_CHECK(datum->quote()->value() == value);
        BOOST_CHECK(datum->instrumentType() == ore::data::MarketDatum::InstrumentType::CAPFLOOR);
        BOOST_CHECK(datum->quoteType() == ore::data::MarketDatum::QuoteType::RATE_NVOL);

        QuantLib::ext::shared_ptr<ore::data::CapFloorQuote> capfloorVolDatum =
            QuantLib::ext::dynamic_pointer_cast<ore::data::CapFloorQuote>(datum);

        BOOST_CHECK(capfloorVolDatum->ccy() == "USD");
        BOOST_CHECK(capfloorVolDatum->term() == Period(5, Years));
        BOOST_CHECK(capfloorVolDatum->underlying() == Period(3, Months));
        BOOST_CHECK(capfloorVolDatum->atm() == false);
        BOOST_CHECK(capfloorVolDatum->relative() == false);
        BOOST_CHECK_CLOSE(capfloorVolDatum->strike(), 0.0, 1e-12);
    }

    { // test capfloor shifted lognormal vol ATM w/ index name
        Date d(1, Jan, 1990);
        Real value = 0.01;

        string input = "CAPFLOOR/RATE_SLNVOL/JPY/EYTIBOR/5Y/3M/1/1/0.0075";

        QuantLib::ext::shared_ptr<ore::data::MarketDatum> datum = ore::data::parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->asofDate() == d);
        BOOST_CHECK(datum->quote()->value() == value);
        BOOST_CHECK(datum->instrumentType() == ore::data::MarketDatum::InstrumentType::CAPFLOOR);
        BOOST_CHECK(datum->quoteType() == ore::data::MarketDatum::QuoteType::RATE_SLNVOL);

        QuantLib::ext::shared_ptr<ore::data::CapFloorQuote> capfloorVolDatum =
            QuantLib::ext::dynamic_pointer_cast<ore::data::CapFloorQuote>(datum);

        BOOST_CHECK(capfloorVolDatum->ccy() == "JPY");
        BOOST_CHECK(capfloorVolDatum->indexName() == "EYTIBOR");
        BOOST_CHECK(capfloorVolDatum->term() == Period(5, Years));
        BOOST_CHECK(capfloorVolDatum->underlying() == Period(3, Months));
        BOOST_CHECK(capfloorVolDatum->atm() == true);
        BOOST_CHECK(capfloorVolDatum->relative() == true);
        BOOST_CHECK_CLOSE(capfloorVolDatum->strike(), 0.0075, 1e-12);
    }

    { // test capfloor shift
        Date d(1, Jan, 1990);
        Real value = 0.01;

        string input = "CAPFLOOR/SHIFT/USD/5Y";

        QuantLib::ext::shared_ptr<ore::data::MarketDatum> datum = ore::data::parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->asofDate() == d);
        BOOST_CHECK(datum->quote()->value() == value);
        BOOST_CHECK(datum->instrumentType() == ore::data::MarketDatum::InstrumentType::CAPFLOOR);
        BOOST_CHECK(datum->quoteType() == ore::data::MarketDatum::QuoteType::SHIFT);

        QuantLib::ext::shared_ptr<ore::data::CapFloorShiftQuote> capfloorShiftDatum =
            QuantLib::ext::dynamic_pointer_cast<ore::data::CapFloorShiftQuote>(datum);

        BOOST_CHECK(capfloorShiftDatum->ccy() == "USD");
        BOOST_CHECK(capfloorShiftDatum->indexTenor() == Period(5, Years));
    }

    { // test capfloor shift w/name
        Date d(1, Jan, 1990);
        Real value = 0.01;

        string input = "CAPFLOOR/SHIFT/JPY/EYTIBOR/5Y";

        QuantLib::ext::shared_ptr<ore::data::MarketDatum> datum = ore::data::parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->asofDate() == d);
        BOOST_CHECK(datum->quote()->value() == value);
        BOOST_CHECK(datum->instrumentType() == ore::data::MarketDatum::InstrumentType::CAPFLOOR);
        BOOST_CHECK(datum->quoteType() == ore::data::MarketDatum::QuoteType::SHIFT);

        QuantLib::ext::shared_ptr<ore::data::CapFloorShiftQuote> capfloorShiftDatum =
            QuantLib::ext::dynamic_pointer_cast<ore::data::CapFloorShiftQuote>(datum);

        BOOST_CHECK(capfloorShiftDatum->ccy() == "JPY");
        BOOST_CHECK(capfloorShiftDatum->indexName() == "EYTIBOR");
        BOOST_CHECK(capfloorShiftDatum->indexTenor() == Period(5, Years));
    }

    { // test capfloor price ATM
        Date d(1, Jan, 1990);
        Real value = 0.01;

        string input = "CAPFLOOR/PRICE/USD/5Y/3M/0/0/0/C";

        QuantLib::ext::shared_ptr<ore::data::MarketDatum> datum = ore::data::parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->asofDate() == d);
        BOOST_CHECK(datum->quote()->value() == value);
        BOOST_CHECK(datum->instrumentType() == ore::data::MarketDatum::InstrumentType::CAPFLOOR);
        BOOST_CHECK(datum->quoteType() == ore::data::MarketDatum::QuoteType::PRICE);

        QuantLib::ext::shared_ptr<ore::data::CapFloorQuote> capfloorPremiumDatum =
            QuantLib::ext::dynamic_pointer_cast<ore::data::CapFloorQuote>(datum);

        BOOST_CHECK(capfloorPremiumDatum->ccy() == "USD");
        BOOST_CHECK(capfloorPremiumDatum->term() == Period(5, Years));
        BOOST_CHECK(capfloorPremiumDatum->underlying() == Period(3, Months));
        BOOST_CHECK(capfloorPremiumDatum->atm() == false);
        BOOST_CHECK(capfloorPremiumDatum->relative() == false);
        BOOST_CHECK_CLOSE(capfloorPremiumDatum->strike(), 0.0, 1e-12);
        BOOST_CHECK(capfloorPremiumDatum->isCap() == true);
    }

    { // test capfloor shifted lognormal vol ATM w/ index name
        Date d(1, Jan, 1990);
        Real value = 0.01;

        string input = "CAPFLOOR/PRICE/JPY/EYTIBOR/5Y/3M/1/1/-0.0075/F";

        QuantLib::ext::shared_ptr<ore::data::MarketDatum> datum = ore::data::parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->asofDate() == d);
        BOOST_CHECK(datum->quote()->value() == value);
        BOOST_CHECK(datum->instrumentType() == ore::data::MarketDatum::InstrumentType::CAPFLOOR);
        BOOST_CHECK(datum->quoteType() == ore::data::MarketDatum::QuoteType::PRICE);

        QuantLib::ext::shared_ptr<ore::data::CapFloorQuote> capfloorVolDatum =
            QuantLib::ext::dynamic_pointer_cast<ore::data::CapFloorQuote>(datum);

        BOOST_CHECK(capfloorVolDatum->ccy() == "JPY");
        BOOST_CHECK(capfloorVolDatum->indexName() == "EYTIBOR");
        BOOST_CHECK(capfloorVolDatum->term() == Period(5, Years));
        BOOST_CHECK(capfloorVolDatum->underlying() == Period(3, Months));
        BOOST_CHECK(capfloorVolDatum->atm() == true);
        BOOST_CHECK(capfloorVolDatum->relative() == true);
        BOOST_CHECK_CLOSE(capfloorVolDatum->strike(), -0.0075, 1e-12);
        BOOST_CHECK(capfloorVolDatum->isCap() == false);
    }

    { // test parsing throws
        Date d(3, Mar, 2018);
        Real value = 10;

        BOOST_CHECK_THROW(
            ore::data::parseMarketDatum(d, "CAPFLOOR/RATE_LNVOL/JPY/EYTIBOR/fortnight/3M/1/1/0.0075", value),
            QuantLib::Error);
        BOOST_CHECK_THROW(
            ore::data::parseMarketDatum(d, "CAPFLOOR/RATE_LNVOL/JPY/EYTIBOR/5Y/fortnight/1/1/0.0075", value),
            QuantLib::Error);
        BOOST_CHECK_THROW(ore::data::parseMarketDatum(d, "CAPFLOOR/RATE_LNVOL/JPY/EYTIBOR/5Y/3M/2Y/1/0.0075", value),
                          QuantLib::Error);
        BOOST_CHECK_THROW(
            ore::data::parseMarketDatum(d, "CAPFLOOR/RATE_LNVOL/JPY/EYTIBOR/5Y/3M/1/string/0.0075", value),
            QuantLib::Error);
        BOOST_CHECK_THROW(ore::data::parseMarketDatum(d, "CAPFLOOR/PRICE/JPY/EYTIBOR/5Y/3M/1/1/one/F", value),
                          QuantLib::Error);
        BOOST_CHECK_THROW(ore::data::parseMarketDatum(d, "CAPFLOOR/PRICE/JPY/EYTIBOR/5Y/3M/1/1/0.0075/straddle", value),
                          QuantLib::Error);
        BOOST_CHECK_THROW(ore::data::parseMarketDatum(d, "CAPFLOOR/PRICE/JPY/EYTIBOR/5Y/3M/1/1/0.0", value),
                          QuantLib::Error);
    }

    BOOST_TEST_MESSAGE("Testing swaption market datum parsing...");

    { // test swaption normal vol ATM
        Date d(1, Jan, 1990);
        Real value = 0.01;

        string input = "SWAPTION/RATE_NVOL/EUR/10Y/30Y/ATM";

        QuantLib::ext::shared_ptr<ore::data::MarketDatum> datum = ore::data::parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->instrumentType() == ore::data::MarketDatum::InstrumentType::SWAPTION);
        BOOST_CHECK(datum->quoteType() == ore::data::MarketDatum::QuoteType::RATE_NVOL);

        QuantLib::ext::shared_ptr<ore::data::SwaptionQuote> swaptionVolDatum =
            QuantLib::ext::dynamic_pointer_cast<ore::data::SwaptionQuote>(datum);

        BOOST_CHECK(swaptionVolDatum->ccy() == "EUR");
        BOOST_CHECK(swaptionVolDatum->expiry() == Period(10, Years));
        BOOST_CHECK(swaptionVolDatum->term() == Period(30, Years));
        BOOST_CHECK(swaptionVolDatum->dimension() == "ATM");
        BOOST_CHECK_CLOSE(swaptionVolDatum->strike(), 0.0, 1e-12);
        BOOST_CHECK(swaptionVolDatum->quoteTag() == "");
    }

    { // test swaption normal vol smile
        Date d(1, Jan, 1990);
        Real value = 0.01;

        string input = "SWAPTION/RATE_NVOL/EUR/EURIBOR/10Y/30Y/Smile/-0.0025";

        QuantLib::ext::shared_ptr<ore::data::MarketDatum> datum = ore::data::parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->instrumentType() == ore::data::MarketDatum::InstrumentType::SWAPTION);
        BOOST_CHECK(datum->quoteType() == ore::data::MarketDatum::QuoteType::RATE_NVOL);

        QuantLib::ext::shared_ptr<ore::data::SwaptionQuote> swaptionVolDatum =
            QuantLib::ext::dynamic_pointer_cast<ore::data::SwaptionQuote>(datum);

        BOOST_CHECK(swaptionVolDatum->ccy() == "EUR");
        BOOST_CHECK(swaptionVolDatum->expiry() == Period(10, Years));
        BOOST_CHECK(swaptionVolDatum->term() == Period(30, Years));
        BOOST_CHECK(swaptionVolDatum->dimension() == "Smile");
        BOOST_CHECK_CLOSE(swaptionVolDatum->strike(), -0.0025, 1e-12);
        BOOST_CHECK(swaptionVolDatum->quoteTag() == "EURIBOR");
    }

    { // test swaption shifted lognormal vol smile
        Date d(1, Jan, 1990);
        Real value = 0.01;

        string input = "SWAPTION/RATE_SLNVOL/EUR/EURIBOR/10Y/30Y/Smile/-0.0025";

        QuantLib::ext::shared_ptr<ore::data::MarketDatum> datum = ore::data::parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->asofDate() == d);
        BOOST_CHECK(datum->quote()->value() == value);
        BOOST_CHECK(datum->instrumentType() == ore::data::MarketDatum::InstrumentType::SWAPTION);
        BOOST_CHECK(datum->quoteType() == ore::data::MarketDatum::QuoteType::RATE_SLNVOL);

        QuantLib::ext::shared_ptr<ore::data::SwaptionQuote> swaptionVolDatum =
            QuantLib::ext::dynamic_pointer_cast<ore::data::SwaptionQuote>(datum);

        BOOST_CHECK(swaptionVolDatum->ccy() == "EUR");
        BOOST_CHECK(swaptionVolDatum->expiry() == Period(10, Years));
        BOOST_CHECK(swaptionVolDatum->term() == Period(30, Years));
        BOOST_CHECK(swaptionVolDatum->dimension() == "Smile");
        BOOST_CHECK_CLOSE(swaptionVolDatum->strike(), -0.0025, 1e-12);
        BOOST_CHECK(swaptionVolDatum->quoteTag() == "EURIBOR");
    }

    { // test swaption shift
        Date d(1, Jan, 1990);
        Real value = 0.01;

        string input = "SWAPTION/SHIFT/EUR/EURIBOR/30Y";

        QuantLib::ext::shared_ptr<ore::data::MarketDatum> datum = ore::data::parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->instrumentType() == ore::data::MarketDatum::InstrumentType::SWAPTION);
        BOOST_CHECK(datum->quoteType() == ore::data::MarketDatum::QuoteType::SHIFT);

        QuantLib::ext::shared_ptr<ore::data::SwaptionShiftQuote> swaptionShiftDatum =
            QuantLib::ext::dynamic_pointer_cast<ore::data::SwaptionShiftQuote>(datum);

        BOOST_CHECK(swaptionShiftDatum->ccy() == "EUR");
        BOOST_CHECK(swaptionShiftDatum->term() == Period(30, Years));
        BOOST_CHECK(swaptionShiftDatum->quoteTag() == "EURIBOR");
    }

    { // test payer swaption ATM premium
        Date d(1, Jan, 1990);
        Real value = 0.01;

        string input = "SWAPTION/PRICE/EUR/10Y/30Y/ATM/P";

        QuantLib::ext::shared_ptr<ore::data::MarketDatum> datum = ore::data::parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->instrumentType() == ore::data::MarketDatum::InstrumentType::SWAPTION);
        BOOST_CHECK(datum->quoteType() == ore::data::MarketDatum::QuoteType::PRICE);

        QuantLib::ext::shared_ptr<ore::data::SwaptionQuote> swaptionPremiumDatum =
            QuantLib::ext::dynamic_pointer_cast<ore::data::SwaptionQuote>(datum);

        BOOST_CHECK(swaptionPremiumDatum->ccy() == "EUR");
        BOOST_CHECK(swaptionPremiumDatum->expiry() == Period(10, Years));
        BOOST_CHECK(swaptionPremiumDatum->term() == Period(30, Years));
        BOOST_CHECK(swaptionPremiumDatum->dimension() == "ATM");
        BOOST_CHECK_CLOSE(swaptionPremiumDatum->strike(), 0.0, 1e-12);
        BOOST_CHECK(swaptionPremiumDatum->quoteTag() == "");
        BOOST_CHECK(swaptionPremiumDatum->isPayer() == true);
    }

    { // test receiver swaption smile premium
        Date d(1, Jan, 1990);
        Real value = 0.01;

        string input = "SWAPTION/PRICE/EUR/EURIBOR/10Y/30Y/Smile/-0.0025/R";

        QuantLib::ext::shared_ptr<ore::data::MarketDatum> datum = ore::data::parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->instrumentType() == ore::data::MarketDatum::InstrumentType::SWAPTION);
        BOOST_CHECK(datum->quoteType() == ore::data::MarketDatum::QuoteType::PRICE);

        QuantLib::ext::shared_ptr<ore::data::SwaptionQuote> swaptionPremiumDatum =
            QuantLib::ext::dynamic_pointer_cast<ore::data::SwaptionQuote>(datum);

        BOOST_CHECK(swaptionPremiumDatum->ccy() == "EUR");
        BOOST_CHECK(swaptionPremiumDatum->expiry() == Period(10, Years));
        BOOST_CHECK(swaptionPremiumDatum->term() == Period(30, Years));
        BOOST_CHECK(swaptionPremiumDatum->dimension() == "Smile");
        BOOST_CHECK_CLOSE(swaptionPremiumDatum->strike(), -0.0025, 1e-12);
        BOOST_CHECK(swaptionPremiumDatum->quoteTag() == "EURIBOR");
        BOOST_CHECK(swaptionPremiumDatum->isPayer() == false);
        BOOST_CHECK_THROW(ore::data::parseMarketDatum(d, "SWAPTION/543/EUR/EURIBOR/10Y/30Y/Smile/-0.0025", value),
                          QuantLib::Error);
        BOOST_CHECK_THROW(
            ore::data::parseMarketDatum(d, "SWAPTION/RATE_SLNVOL/EUR/EURIBOR/TodayWasGonna/30Y/Smile/-0.0025", value),
            QuantLib::Error);
        BOOST_CHECK_THROW(
            ore::data::parseMarketDatum(d, "SWAPTION/RATE_SLNVOL/EUR/EURIBOR/10Y/BeTheDay/Smile/-0.0025", value),
            QuantLib::Error);
        BOOST_CHECK_THROW(
            ore::data::parseMarketDatum(d, "SWAPTION/RATE_SLNVOL/EUR/EURIBOR/10Y/30Y/ButTheyll/-0.0025", value),
            QuantLib::Error);
        BOOST_CHECK_THROW(
            ore::data::parseMarketDatum(d, "SWAPTION/RATE_SLNVOL/EUR/EURIBOR/10Y/30Y/Smile/NeverThrowIt", value),
            QuantLib::Error);
        BOOST_CHECK_THROW(
            ore::data::parseMarketDatum(d, "SWAPTION/RATE_SLNVOL/EUR/EURIBOR/10Y/30Y/Smile/0.001/BackToYou", value),
            QuantLib::Error);
    }

    BOOST_TEST_MESSAGE("Testing correlation market datum parsing...");

    { // test rate quote
        Date d(1, Jan, 1990);
        Real value = 1;

        string input = "CORRELATION/RATE/INDEX1/INDEX2/1Y/ATM";

        QuantLib::ext::shared_ptr<ore::data::MarketDatum> datum = ore::data::parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->asofDate() == d);
        BOOST_CHECK(datum->quote()->value() == value);
        BOOST_CHECK(datum->instrumentType() == ore::data::MarketDatum::InstrumentType::CORRELATION);
        BOOST_CHECK(datum->quoteType() == ore::data::MarketDatum::QuoteType::RATE);

        QuantLib::ext::shared_ptr<ore::data::CorrelationQuote> spreadDatum =
            QuantLib::ext::dynamic_pointer_cast<ore::data::CorrelationQuote>(datum);

        BOOST_CHECK(spreadDatum->index1() == "INDEX1");
        BOOST_CHECK(spreadDatum->index2() == "INDEX2");
        BOOST_CHECK(spreadDatum->expiry() == "1Y");
        BOOST_CHECK(spreadDatum->strike() == "ATM");
    }

    { // test price quote
        Date d(3, Mar, 2018);
        Real value = 10;

        string input = "CORRELATION/PRICE/INDEX1/INDEX2/1Y/0.1";

        QuantLib::ext::shared_ptr<ore::data::MarketDatum> datum = ore::data::parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->asofDate() == d);
        BOOST_CHECK(datum->quote()->value() == value);
        BOOST_CHECK(datum->instrumentType() == ore::data::MarketDatum::InstrumentType::CORRELATION);
        BOOST_CHECK(datum->quoteType() == ore::data::MarketDatum::QuoteType::PRICE);

        QuantLib::ext::shared_ptr<ore::data::CorrelationQuote> spreadDatum =
            QuantLib::ext::dynamic_pointer_cast<ore::data::CorrelationQuote>(datum);

        BOOST_CHECK(spreadDatum->index1() == "INDEX1");
        BOOST_CHECK(spreadDatum->index2() == "INDEX2");
        BOOST_CHECK(spreadDatum->expiry() == "1Y");
        BOOST_CHECK(spreadDatum->strike() == "0.1");
    }

    { // test parsing throws
        Date d(3, Mar, 2018);
        Real value = 10;

        BOOST_CHECK_THROW(ore::data::parseMarketDatum(d, "CORRELATION/PRICE/INDEX1/INDEX2/1Y/SS", value),
                          QuantLib::Error);
        BOOST_CHECK_THROW(ore::data::parseMarketDatum(d, "CORRELATION/PRICE/INDEX1/INDEX2/6X/0.1", value),
                          QuantLib::Error);
    }

    BOOST_TEST_MESSAGE("Testing commodity spot market datum parsing...");

    // test normal parsing
    {
        Date d(29, Jul, 2019);
        Real value = 1418.1;

        string input = "COMMODITY/PRICE/PM:XAUUSD/USD";

        QuantLib::ext::shared_ptr<MarketDatum> datum = parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->asofDate() == d);
        BOOST_CHECK(datum->quote()->value() == value);
        BOOST_CHECK(datum->instrumentType() == MarketDatum::InstrumentType::COMMODITY_SPOT);
        BOOST_CHECK(datum->quoteType() == MarketDatum::QuoteType::PRICE);

        QuantLib::ext::shared_ptr<CommoditySpotQuote> q = QuantLib::ext::dynamic_pointer_cast<CommoditySpotQuote>(datum);

        BOOST_CHECK(q->commodityName() == "PM:XAUUSD");
        BOOST_CHECK(q->quoteCurrency() == "USD");
    }

    // test possible exceptions
    {
        Date d(29, Jul, 2019);
        Real value = 1418.1;

        BOOST_CHECK_THROW(parseMarketDatum(d, "COMMODITY_SPOT/PRICE/PM:XAUUSD/USD", value), Error);
        BOOST_CHECK_THROW(parseMarketDatum(d, "COMMODITY/RATE/PM:XAUUSD/USD", value), Error);
        BOOST_CHECK_THROW(parseMarketDatum(d, "COMMODITY/PRICE/USD", value), Error);
    }

    BOOST_TEST_MESSAGE("Testing commodity forward market datum parsing...");

    // test normal parsing
    {
        Date d(29, Jul, 2019);
        Real value = 300.16535;

        // Tenor based quote
        string input = "COMMODITY_FWD/PRICE/PM:XAUUSD/USD/1M";
        QuantLib::ext::shared_ptr<MarketDatum> datum = parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->asofDate() == d);
        BOOST_CHECK(datum->quote()->value() == value);
        BOOST_CHECK(datum->instrumentType() == MarketDatum::InstrumentType::COMMODITY_FWD);
        BOOST_CHECK(datum->quoteType() == MarketDatum::QuoteType::PRICE);

        QuantLib::ext::shared_ptr<CommodityForwardQuote> q = QuantLib::ext::dynamic_pointer_cast<CommodityForwardQuote>(datum);
        BOOST_CHECK(q->commodityName() == "PM:XAUUSD");
        BOOST_CHECK(q->quoteCurrency() == "USD");
        BOOST_CHECK(q->tenorBased());
        BOOST_CHECK(q->expiryDate() == Date());
        BOOST_CHECK(q->tenor() == 1 * Months);
        BOOST_CHECK(q->startTenor() == boost::none);

        // Date based quote
        input = "COMMODITY_FWD/PRICE/PM:XAUUSD/USD/2019-08-30";
        datum = parseMarketDatum(d, input, value);
        q = QuantLib::ext::dynamic_pointer_cast<CommodityForwardQuote>(datum);
        BOOST_CHECK(q->commodityName() == "PM:XAUUSD");
        BOOST_CHECK(q->quoteCurrency() == "USD");
        BOOST_CHECK(!q->tenorBased());
        BOOST_CHECK(q->expiryDate() == Date(30, Aug, 2019));
        BOOST_CHECK(q->tenor() == Period());
        BOOST_CHECK(q->startTenor() == boost::none);

        // Special tenor based quotes

        // Overnight
        input = "COMMODITY_FWD/PRICE/PM:XAUUSD/USD/ON";
        datum = parseMarketDatum(d, input, value);
        q = QuantLib::ext::dynamic_pointer_cast<CommodityForwardQuote>(datum);
        BOOST_CHECK(q->tenorBased());
        BOOST_CHECK(q->expiryDate() == Date());
        BOOST_CHECK(q->tenor() == 1 * Days);
        BOOST_CHECK(q->startTenor() == 0 * Days);

        // Tom-next
        input = "COMMODITY_FWD/PRICE/PM:XAUUSD/USD/TN";
        datum = parseMarketDatum(d, input, value);
        q = QuantLib::ext::dynamic_pointer_cast<CommodityForwardQuote>(datum);
        BOOST_CHECK(q->tenorBased());
        BOOST_CHECK(q->expiryDate() == Date());
        BOOST_CHECK(q->tenor() == 1 * Days);
        BOOST_CHECK(q->startTenor() == 1 * Days);

        // Spot-next
        input = "COMMODITY_FWD/PRICE/PM:XAUUSD/USD/SN";
        datum = parseMarketDatum(d, input, value);
        q = QuantLib::ext::dynamic_pointer_cast<CommodityForwardQuote>(datum);
        BOOST_CHECK(q->tenorBased());
        BOOST_CHECK(q->expiryDate() == Date());
        BOOST_CHECK(q->tenor() == 1 * Days);
        BOOST_CHECK(q->startTenor() == boost::none);
    }

    // test possible exceptions
    {
        Date d(29, Jul, 2019);
        Real value = 300.16535;

        BOOST_CHECK_THROW(parseMarketDatum(d, "COMMODITY_FORWARD/PRICE/PM:XAUUSD/USD/1M", value), Error);
        BOOST_CHECK_THROW(parseMarketDatum(d, "COMMODITY_FWD/RATE/PM:XAUUSD/USD/1M", value), Error);
        BOOST_CHECK_THROW(parseMarketDatum(d, "COMMODITY_FWD/PRICE/USD/1M", value), Error);
        BOOST_CHECK_THROW(parseMarketDatum(d, "COMMODITY_FWD/PRICE/PM:XAUUSD/USD/2019-12", value), Error);
    }

    BOOST_TEST_MESSAGE("Testing fx option market datum parsing...");

    // test normal parsing
    {
        Date d(29, Jul, 2019);
        Real value = 1.234;

        // ATM quote
        string input = "FX_OPTION/RATE_LNVOL/EUR/USD/1M/ATM";
        QuantLib::ext::shared_ptr<MarketDatum> datum = parseMarketDatum(d, input, value);

        BOOST_CHECK(datum->asofDate() == d);
        BOOST_CHECK(datum->quote()->value() == value);
        BOOST_CHECK(datum->instrumentType() == MarketDatum::InstrumentType::FX_OPTION);
        BOOST_CHECK(datum->quoteType() == MarketDatum::QuoteType::RATE_LNVOL);

        QuantLib::ext::shared_ptr<FXOptionQuote> q = QuantLib::ext::dynamic_pointer_cast<FXOptionQuote>(datum);
        BOOST_CHECK(q->unitCcy() == "EUR");
        BOOST_CHECK(q->ccy() == "USD");
        BOOST_CHECK(q->expiry() == Period(1, Months));
        BOOST_CHECK(q->strike() == "ATM");

        // Butterfly quote
        input = "FX_OPTION/RATE_LNVOL/EUR/USD/2M/25BF";
        datum = parseMarketDatum(d, input, value);
        q = QuantLib::ext::dynamic_pointer_cast<FXOptionQuote>(datum);
        BOOST_CHECK(q->unitCcy() == "EUR");
        BOOST_CHECK(q->ccy() == "USD");
        BOOST_CHECK(q->expiry() == Period(2, Months));
        BOOST_CHECK(q->strike() == "25BF");

        input = "FX_OPTION/RATE_LNVOL/EUR/USD/2M/10BF";
        datum = parseMarketDatum(d, input, value);
        q = QuantLib::ext::dynamic_pointer_cast<FXOptionQuote>(datum);
        BOOST_CHECK(q->unitCcy() == "EUR");
        BOOST_CHECK(q->ccy() == "USD");
        BOOST_CHECK(q->expiry() == Period(2, Months));
        BOOST_CHECK(q->strike() == "10BF");

        // Risk Reversal quote
        input = "FX_OPTION/RATE_LNVOL/EUR/USD/2M/25RR";
        datum = parseMarketDatum(d, input, value);
        q = QuantLib::ext::dynamic_pointer_cast<FXOptionQuote>(datum);
        BOOST_CHECK(q->unitCcy() == "EUR");
        BOOST_CHECK(q->ccy() == "USD");
        BOOST_CHECK(q->expiry() == Period(2, Months));
        BOOST_CHECK(q->strike() == "25RR");

        input = "FX_OPTION/RATE_LNVOL/EUR/USD/2M/10RR";
        datum = parseMarketDatum(d, input, value);
        q = QuantLib::ext::dynamic_pointer_cast<FXOptionQuote>(datum);
        BOOST_CHECK(q->unitCcy() == "EUR");
        BOOST_CHECK(q->ccy() == "USD");
        BOOST_CHECK(q->expiry() == Period(2, Months));
        BOOST_CHECK(q->strike() == "10RR");

        // Strike based quote
        input = "FX_OPTION/RATE_LNVOL/EUR/USD/2M/10C";
        datum = parseMarketDatum(d, input, value);
        q = QuantLib::ext::dynamic_pointer_cast<FXOptionQuote>(datum);
        BOOST_CHECK(q->unitCcy() == "EUR");
        BOOST_CHECK(q->ccy() == "USD");
        BOOST_CHECK(q->expiry() == Period(2, Months));
        BOOST_CHECK(q->strike() == "10C");

        input = "FX_OPTION/RATE_LNVOL/EUR/USD/2M/20P";
        datum = parseMarketDatum(d, input, value);
        q = QuantLib::ext::dynamic_pointer_cast<FXOptionQuote>(datum);
        BOOST_CHECK(q->unitCcy() == "EUR");
        BOOST_CHECK(q->ccy() == "USD");
        BOOST_CHECK(q->expiry() == Period(2, Months));
        BOOST_CHECK(q->strike() == "20P");

        // test possible exceptions
        {
            Date d(29, Jul, 2019);
            Real value = 300.16535;

            BOOST_CHECK_THROW(parseMarketDatum(d, "FX_OPTION/RATE_LNVOL/EUR/USD/1M/ATMF", value), Error);
            BOOST_CHECK_THROW(parseMarketDatum(d, "FX_OPTION/RATE_LNVOL/EUR/USD/1M/BBFF", value), Error);
            BOOST_CHECK_THROW(parseMarketDatum(d, "FX_OPTION/RATE_LNVOL/EUR/USD/1M/1LRR", value), Error);
            BOOST_CHECK_THROW(parseMarketDatum(d, "FX_OPTION/RATE_LNVOL/EUR/USD/1M/10D", value), Error);
            BOOST_CHECK_THROW(parseMarketDatum(d, "FX_OPTION/RATE_LNVOL/EUR/USD/1M", value), Error);
            BOOST_CHECK_THROW(parseMarketDatum(d, "FX_OPTION/RATE_LNVOL/EUR/USD/2019-12", value), Error);
        }
    }
}

BOOST_AUTO_TEST_CASE(testJointCalendar) {

    std::vector<Calendar> cals;
    std::set<Date> expectedHolidays;
    // add peruvian holidays
    expectedHolidays.insert(Date(1, January, 2018));
    expectedHolidays.insert(Date(29, March, 2018));
    expectedHolidays.insert(Date(30, March, 2018));
    expectedHolidays.insert(Date(1, May, 2018));
    expectedHolidays.insert(Date(29, June, 2018));
    expectedHolidays.insert(Date(27, July, 2018));
    expectedHolidays.insert(Date(30, August, 2018));
    expectedHolidays.insert(Date(31, August, 2018));
    expectedHolidays.insert(Date(8, October, 2018));
    expectedHolidays.insert(Date(1, November, 2018));
    expectedHolidays.insert(Date(2, November, 2018));
    expectedHolidays.insert(Date(25, December, 2018));

    Calendar peru = QuantExt::Peru();

    cals.push_back(peru);
    Calendar joint1 = QuantLib::JointCalendar(cals);

    std::vector<Date> hol = joint1.holidayList(Date(1, January, 2018), Date(31, December, 2018));
    BOOST_CHECK(hol.size() == expectedHolidays.size());

    checkCalendars(expectedHolidays, hol);

    // add columbian holidays
    expectedHolidays.insert(Date(1, January, 2018));
    expectedHolidays.insert(Date(8, January, 2018));
    expectedHolidays.insert(Date(19, March, 2018));
    expectedHolidays.insert(Date(29, March, 2018));
    expectedHolidays.insert(Date(30, March, 2018));
    expectedHolidays.insert(Date(1, May, 2018));
    expectedHolidays.insert(Date(14, May, 2018));
    expectedHolidays.insert(Date(4, June, 2018));
    expectedHolidays.insert(Date(11, June, 2018));
    expectedHolidays.insert(Date(2, July, 2018));
    expectedHolidays.insert(Date(20, July, 2018));
    expectedHolidays.insert(Date(7, August, 2018));
    expectedHolidays.insert(Date(20, August, 2018));
    expectedHolidays.insert(Date(15, October, 2018));
    expectedHolidays.insert(Date(5, November, 2018));
    expectedHolidays.insert(Date(12, November, 2018));
    expectedHolidays.insert(Date(25, December, 2018));

    Calendar col = Colombia();
    cals.push_back(col);
    Calendar joint2 = QuantLib::JointCalendar(cals);

    hol = joint2.holidayList(Date(1, January, 2018), Date(31, December, 2018));
    BOOST_CHECK(hol.size() == expectedHolidays.size());
    checkCalendars(expectedHolidays, hol);

    // add philippines holidays
    expectedHolidays.insert(Date(1, January, 2018));
    expectedHolidays.insert(Date(2, January, 2018));
    expectedHolidays.insert(Date(29, March, 2018));
    expectedHolidays.insert(Date(30, March, 2018));
    expectedHolidays.insert(Date(9, April, 2018));
    expectedHolidays.insert(Date(1, May, 2018));
    expectedHolidays.insert(Date(12, June, 2018));
    expectedHolidays.insert(Date(21, August, 2018));
    expectedHolidays.insert(Date(27, August, 2018));
    expectedHolidays.insert(Date(1, November, 2018));
    expectedHolidays.insert(Date(30, November, 2018));
    expectedHolidays.insert(Date(25, December, 2018));
    expectedHolidays.insert(Date(31, December, 2018));

    Calendar phil = Philippines();
    cals.push_back(phil);
    Calendar joint3 = QuantLib::JointCalendar(cals);

    hol = joint3.holidayList(Date(1, January, 2018), Date(31, December, 2018));
    BOOST_CHECK(hol.size() == expectedHolidays.size());
    checkCalendars(expectedHolidays, hol);

    // add thailand holidays
    expectedHolidays.insert(Date(1, January, 2018));
    expectedHolidays.insert(Date(2, January, 2018));
    expectedHolidays.insert(Date(1, March, 2018)); // Makha Bucha Day
    expectedHolidays.insert(Date(6, April, 2018));
    expectedHolidays.insert(Date(13, April, 2018));
    expectedHolidays.insert(Date(16, April, 2018));
    expectedHolidays.insert(Date(1, May, 2018));
    expectedHolidays.insert(Date(29, May, 2018));  // Wisakha Bucha Day
    expectedHolidays.insert(Date(27, July, 2018)); // Asarnha Bucha Day
    expectedHolidays.insert(Date(30, July, 2018));
    expectedHolidays.insert(Date(13, August, 2018));
    expectedHolidays.insert(Date(15, October, 2018));
    expectedHolidays.insert(Date(23, October, 2018));
    expectedHolidays.insert(Date(5, December, 2018));
    expectedHolidays.insert(Date(10, December, 2018));
    expectedHolidays.insert(Date(31, December, 2018));

    Calendar thai = Thailand();
    cals.push_back(thai);
    Calendar joint4 = QuantLib::JointCalendar(cals);

    hol = joint4.holidayList(Date(1, January, 2018), Date(31, December, 2018));
    BOOST_CHECK(hol.size() == expectedHolidays.size());
    checkCalendars(expectedHolidays, hol);

    // add malaysia holidays
    expectedHolidays.insert(Date(1, January, 2018));
    expectedHolidays.insert(Date(1, February, 2018));
    expectedHolidays.insert(Date(1, May, 2018));
    expectedHolidays.insert(Date(31, August, 2018));
    expectedHolidays.insert(Date(17, September, 2018));
    expectedHolidays.insert(Date(25, December, 2018));

    Calendar mal = Malaysia();
    cals.push_back(mal);
    Calendar joint5 = QuantLib::JointCalendar(cals);

    hol = joint5.holidayList(Date(1, January, 2018), Date(31, December, 2018));
    BOOST_CHECK(hol.size() == expectedHolidays.size());
    checkCalendars(expectedHolidays, hol);

    // add chilean calendar
    expectedHolidays.insert(Date(1, January, 2018));
    expectedHolidays.insert(Date(30, March, 2018));
    expectedHolidays.insert(Date(1, May, 2018));
    expectedHolidays.insert(Date(21, May, 2018));
    expectedHolidays.insert(Date(2, July, 2018));
    expectedHolidays.insert(Date(16, July, 2018));
    expectedHolidays.insert(Date(15, August, 2018));
    expectedHolidays.insert(Date(17, September, 2018));
    expectedHolidays.insert(Date(18, September, 2018));
    expectedHolidays.insert(Date(19, September, 2018));
    expectedHolidays.insert(Date(15, October, 2018));
    expectedHolidays.insert(Date(1, November, 2018));
    expectedHolidays.insert(Date(2, November, 2018));
    expectedHolidays.insert(Date(25, December, 2018));

    Calendar chil = Chile();
    cals.push_back(chil);
    Calendar joint6 = QuantLib::JointCalendar(cals);

    hol = joint6.holidayList(Date(1, January, 2018), Date(31, December, 2018));
    BOOST_CHECK(hol.size() == expectedHolidays.size());
    checkCalendars(expectedHolidays, hol);

    // add netherlands calendar
    expectedHolidays.insert(Date(1, January, 2018));
    expectedHolidays.insert(Date(30, March, 2018));
    expectedHolidays.insert(Date(2, April, 2018));
    expectedHolidays.insert(Date(27, April, 2018));
    expectedHolidays.insert(Date(10, May, 2018));
    expectedHolidays.insert(Date(21, May, 2018));
    expectedHolidays.insert(Date(25, December, 2018));
    expectedHolidays.insert(Date(26, December, 2018));

    Calendar net = Netherlands();
    cals.push_back(net);
    Calendar joint7 = QuantLib::JointCalendar(cals);

    hol = joint7.holidayList(Date(1, January, 2018), Date(31, December, 2018));
    BOOST_CHECK(hol.size() == expectedHolidays.size());
    checkCalendars(expectedHolidays, hol);

    // add French calendar
    expectedHolidays.insert(Date(1, January, 2018));
    expectedHolidays.insert(Date(30, March, 2018));
    expectedHolidays.insert(Date(2, April, 2018));
    expectedHolidays.insert(Date(1, May, 2018));
    expectedHolidays.insert(Date(8, May, 2018));
    expectedHolidays.insert(Date(10, May, 2018));
    expectedHolidays.insert(Date(21, May, 2018));
    expectedHolidays.insert(Date(15, August, 2018));
    expectedHolidays.insert(Date(1, November, 2018));
    expectedHolidays.insert(Date(25, December, 2018));
    expectedHolidays.insert(Date(26, December, 2018));

    Calendar fre = France();
    cals.push_back(fre);
    Calendar joint8 = QuantLib::JointCalendar(cals);

    hol = joint8.holidayList(Date(1, January, 2018), Date(31, December, 2018));
    BOOST_CHECK(hol.size() == expectedHolidays.size());
    checkCalendars(expectedHolidays, hol);
}

BOOST_AUTO_TEST_CASE(testParseBoostAny) {

    BOOST_TEST_MESSAGE("Testing parsing of Boost::Any...");

    // For QuantLib::Array
    Array arr(5, 3);
    boost::any any_array = boost::any_cast<Array>(arr);
    std::pair<std::string, std::string> result;
    BOOST_REQUIRE_NO_THROW(result = ore::data::parseBoostAny(any_array, 0));
    BOOST_CHECK_EQUAL(result.first, "array");
    BOOST_CHECK_EQUAL(result.second, "[ 3; 3; 3; 3; 3 ]");
}

BOOST_AUTO_TEST_CASE(testParseBoostAnyWithCurrency) {

    BOOST_TEST_MESSAGE("Testing parsing of Boost::Any...");

    Currency usd = USDCurrency();
    std::pair<std::string, std::string> result;
    BOOST_REQUIRE_NO_THROW(result = ore::data::parseBoostAny(usd));
    BOOST_CHECK_EQUAL(result.first, "currency");
    BOOST_CHECK_EQUAL(result.second, "USD");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

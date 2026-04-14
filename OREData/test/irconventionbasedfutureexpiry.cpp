/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

// clang-format off
#include <boost/test/unit_test.hpp>
// clang-format on
#include <ored/utilities/toplevelfixture.hpp>

#include <ored/marketdata/expiry.hpp>
#include <ored/utilities/irconventionbasedfutureexpiry.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ql/time/calendars/unitedstates.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace ore::data;

namespace {

// Register a FutureConvention for MM futures (IMM rule, e.g. Euribor-3M)
void setupMmFutureConvention(const string& id, const string& index) {
    auto conventions = QuantLib::ext::make_shared<Conventions>();
    auto conv = QuantLib::ext::make_shared<FutureConvention>(id, index, RateAveraging::Type::Compound,
                                                             FutureConvention::DateGenerationRule::IMM, "");
    conventions->add(conv);
    InstrumentConventions::instance().setConventions(conventions);
}

// Register a FutureConvention for OIS futures (IMM rule, e.g. SOFR-3M)
void setupOisFutureConvention(const string& id, const string& index, const string& tenor) {
    auto conventions = QuantLib::ext::make_shared<Conventions>();
    auto conv = QuantLib::ext::make_shared<FutureConvention>(id, index, RateAveraging::Type::Compound,
                                                             FutureConvention::DateGenerationRule::IMM, "", tenor);
    conventions->add(conv);
    InstrumentConventions::instance().setConventions(conventions);
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::data::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(IrConventionBasedFutureExpiryTests)

// --- MM Future (IMM) tests ---
// IMM dates in 2026:
//   Apr: 2026-04-15, May: 2026-05-20, Jun: 2026-06-17, Jul: 2026-07-15

BOOST_AUTO_TEST_CASE(testMmNextExpiryBeforeImmDate) {
    BOOST_TEST_MESSAGE("Testing MM nextExpiry before IMM date");
    setupMmFutureConvention("EUR-EURIBOR-3M-FUTURE", "EUR-EURIBOR-3M");
    IrConventionBasedFutureExpiry calc("EUR-EURIBOR-3M-FUTURE");

    // April 14 is before April IMM (April 15) -> should return April 15
    Date ref(14, Apr, 2026);
    Date expected(15, Apr, 2026);
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, ref, 0), expected);
    BOOST_CHECK_EQUAL(calc.nextExpiry(false, ref, 0), expected);
}

BOOST_AUTO_TEST_CASE(testMmNextExpiryOnImmDate) {
    BOOST_TEST_MESSAGE("Testing MM nextExpiry on IMM date");
    setupMmFutureConvention("EUR-EURIBOR-3M-FUTURE", "EUR-EURIBOR-3M");
    IrConventionBasedFutureExpiry calc("EUR-EURIBOR-3M-FUTURE");

    Date immDate(15, Apr, 2026);
    Date nextImm(20, May, 2026);

    // includeExpiry=true: on the IMM date itself, return it
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, immDate, 0), immDate);
    // includeExpiry=false: on the IMM date, skip to next month's IMM
    BOOST_CHECK_EQUAL(calc.nextExpiry(false, immDate, 0), nextImm);
}

BOOST_AUTO_TEST_CASE(testMmNextExpiryAfterImmDate) {
    BOOST_TEST_MESSAGE("Testing MM nextExpiry after IMM date (IMM+1)");
    setupMmFutureConvention("EUR-EURIBOR-3M-FUTURE", "EUR-EURIBOR-3M");
    IrConventionBasedFutureExpiry calc("EUR-EURIBOR-3M-FUTURE");

    // April 16 is after April IMM (April 15) -> should return May IMM (May 20)
    Date ref(16, Apr, 2026);
    Date expected(20, May, 2026);
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, ref, 0), expected);
    BOOST_CHECK_EQUAL(calc.nextExpiry(false, ref, 0), expected);
}

BOOST_AUTO_TEST_CASE(testMmNextExpiryWithOffset) {
    BOOST_TEST_MESSAGE("Testing MM nextExpiry with offset (c1, c2, c3)");
    setupMmFutureConvention("EUR-EURIBOR-3M-FUTURE", "EUR-EURIBOR-3M");
    IrConventionBasedFutureExpiry calc("EUR-EURIBOR-3M-FUTURE");

    // Reference date before April IMM
    Date ref(14, Apr, 2026);
    Date aprImm(15, Apr, 2026);
    Date mayImm(20, May, 2026);
    Date junImm(17, Jun, 2026);
    Date julImm(15, Jul, 2026);

    // offset=0 -> c1 (next future) = April IMM
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, ref, 0), aprImm);
    // offset=1 -> c2 (2nd next) = May IMM
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, ref, 1), mayImm);
    // offset=2 -> c3 = June IMM
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, ref, 2), junImm);
    // offset=3 -> c4 = July IMM
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, ref, 3), julImm);
}

BOOST_AUTO_TEST_CASE(testMmNextExpiryWithOffsetOnImmDate) {
    BOOST_TEST_MESSAGE("Testing MM nextExpiry with offset starting on IMM date");
    setupMmFutureConvention("EUR-EURIBOR-3M-FUTURE", "EUR-EURIBOR-3M");
    IrConventionBasedFutureExpiry calc("EUR-EURIBOR-3M-FUTURE");

    Date immDate(15, Apr, 2026);
    Date mayImm(20, May, 2026);
    Date junImm(17, Jun, 2026);
    // includeExpiry=false, offset=0: on the IMM date, skip to next month's IMM -> May IMM
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, immDate, 0), immDate);
    // includeExpiry=false, offset=0: skip today's IMM -> May IMM
    BOOST_CHECK_EQUAL(calc.nextExpiry(false, immDate, 0), mayImm);
    // includeExpiry=true, offset=1: include today's IMM, then step 1 forward -> May IMM
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, immDate, 1), mayImm);
    // includeExpiry=false, offset=1: includeExpiry only applies when offset==0, so step 1 forward from Apr -> May IMM
    BOOST_CHECK_EQUAL(calc.nextExpiry(false, immDate, 1), mayImm);
}

BOOST_AUTO_TEST_CASE(testMmNextExpiryDecemberRollover) {
    BOOST_TEST_MESSAGE("Testing MM nextExpiry December -> January rollover");
    setupMmFutureConvention("EUR-EURIBOR-3M-FUTURE", "EUR-EURIBOR-3M");
    IrConventionBasedFutureExpiry calc("EUR-EURIBOR-3M-FUTURE");

    // December 17, 2026 is after Dec IMM (Dec 16) -> should roll to Jan 2027 IMM (Jan 20)
    Date ref(17, Dec, 2026);
    Date expected(20, Jan, 2027);
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, ref, 0), expected);
}

// --- expiryToIrCurveDate tests ---

BOOST_AUTO_TEST_CASE(testExpiryToIrCurveDatePeriod) {
    BOOST_TEST_MESSAGE("Testing expiryToIrCurveDate with ExpiryPeriod");
    Date ref(14, Apr, 2026);
    auto expiry = 3 * Months;
    Date result = expiryToIrCurveDate(expiry, ref);
    BOOST_CHECK_EQUAL(result, ref + 3 * Months);
}

BOOST_AUTO_TEST_CASE(testExpiryToIrCurveDateAbsoluteDate) {
    BOOST_TEST_MESSAGE("Testing expiryToIrCurveDate with ExpiryDate");
    Date ref(14, Apr, 2026);
    Date target(17, Jun, 2026);
    Date result = expiryToIrCurveDate(target, ref);
    BOOST_CHECK_EQUAL(result, target);
}

BOOST_AUTO_TEST_CASE(testExpiryToIrCurveDateFutureContinuation) {
    BOOST_TEST_MESSAGE("Testing expiryToIrCurveDate with FutureContinuationExpiry");
    setupMmFutureConvention("EUR-EURIBOR-3M-FUTURE", "EUR-EURIBOR-3M");
    IrConventionBasedFutureExpiry calc("EUR-EURIBOR-3M-FUTURE");

    Date ref(14, Apr, 2026);
    Date aprImm(15, Apr, 2026);
    Date mayImm(20, May, 2026);
    Date junImm(17, Jun, 2026);

    // c1 -> next future = April IMM
    auto c1 = FutureContinuationExpiry(1);
    BOOST_CHECK_EQUAL(expiryToIrCurveDate(c1, ref, calc), aprImm);

    // c2 -> 2nd next future = May IMM
    auto c2 = FutureContinuationExpiry(2);
    BOOST_CHECK_EQUAL(expiryToIrCurveDate(c2, ref, calc), mayImm);

    // c3 -> 3rd next future = June IMM
    auto c3 = FutureContinuationExpiry(3);
    BOOST_CHECK_EQUAL(expiryToIrCurveDate(c3, ref, calc), junImm);
}

// --- OIS Future (SOFR-3M, IMM) tests ---
// OIS futures with IMM rule return .second (end date) from getOiFutureStartEndDate
// With 3M tenor, end dates are simply the IMM dates of the contract month
// (same as MM futures in terms of calendar dates)
// IMM dates: Apr 15, May 20, Jun 17, Jul 15, Dec 16 2026, Jan 20 2027

BOOST_AUTO_TEST_CASE(testOisNextExpiryBeforeImmDate) {
    BOOST_TEST_MESSAGE("Testing OIS (SOFR-3M) nextExpiry before IMM date");
    setupOisFutureConvention("USD-SOFR-3M-FUTURE", "USD-SOFR", "3M");
    IrConventionBasedFutureExpiry calc("USD-SOFR-3M-FUTURE");

    // April 14 is before the OIS Apr end date (Apr 15) -> should return April 15
    Date ref(14, Apr, 2026);
    Date expected(15, Apr, 2026);
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, ref, 0), expected);
    BOOST_CHECK_EQUAL(calc.nextExpiry(false, ref, 0), expected);
}

BOOST_AUTO_TEST_CASE(testOisNextExpiryOnImmDate) {
    BOOST_TEST_MESSAGE("Testing OIS (SOFR-3M) nextExpiry on IMM date");
    setupOisFutureConvention("USD-SOFR-3M-FUTURE", "USD-SOFR", "3M");
    IrConventionBasedFutureExpiry calc("USD-SOFR-3M-FUTURE");

    Date immDate(15, Apr, 2026);
    Date nextImm(20, May, 2026);

    // includeExpiry=true: on the end date itself, return it
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, immDate, 0), immDate);
    // includeExpiry=false: on the end date, skip to next
    BOOST_CHECK_EQUAL(calc.nextExpiry(false, immDate, 0), nextImm);
}

BOOST_AUTO_TEST_CASE(testOisNextExpiryAfterImmDate) {
    BOOST_TEST_MESSAGE("Testing OIS (SOFR-3M) nextExpiry after IMM date (IMM+1)");
    setupOisFutureConvention("USD-SOFR-3M-FUTURE", "USD-SOFR", "3M");
    IrConventionBasedFutureExpiry calc("USD-SOFR-3M-FUTURE");

    // April 16 is after Apr end date (Apr 15) -> should return May end (May 20)
    Date ref(16, Apr, 2026);
    Date expected(20, May, 2026);
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, ref, 0), expected);
    BOOST_CHECK_EQUAL(calc.nextExpiry(false, ref, 0), expected);
}

BOOST_AUTO_TEST_CASE(testOisNextExpiryWithOffset) {
    BOOST_TEST_MESSAGE("Testing OIS (SOFR-3M) nextExpiry with offset (c1, c2, c3)");
    setupOisFutureConvention("USD-SOFR-3M-FUTURE", "USD-SOFR", "3M");
    IrConventionBasedFutureExpiry calc("USD-SOFR-3M-FUTURE");

    Date ref(14, Apr, 2026);
    Date aprEnd(15, Apr, 2026);
    Date mayEnd(20, May, 2026);
    Date junEnd(17, Jun, 2026);
    Date julEnd(15, Jul, 2026);

    BOOST_CHECK_EQUAL(calc.nextExpiry(true, ref, 0), aprEnd);
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, ref, 1), mayEnd);
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, ref, 2), junEnd);
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, ref, 3), julEnd);
}

BOOST_AUTO_TEST_CASE(testOisNextExpiryWithOffsetOnImmDate) {
    BOOST_TEST_MESSAGE("Testing OIS (SOFR-3M) nextExpiry with offset on IMM date");
    setupOisFutureConvention("USD-SOFR-3M-FUTURE", "USD-SOFR", "3M");
    IrConventionBasedFutureExpiry calc("USD-SOFR-3M-FUTURE");

    Date immDate(15, Apr, 2026);
    Date mayEnd(20, May, 2026);
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, immDate, 0), immDate);
    BOOST_CHECK_EQUAL(calc.nextExpiry(false, immDate, 0), mayEnd);
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, immDate, 1), mayEnd);
    BOOST_CHECK_EQUAL(calc.nextExpiry(false, immDate, 1), mayEnd);
}

BOOST_AUTO_TEST_CASE(testOisNextExpiryDecemberRollover) {
    BOOST_TEST_MESSAGE("Testing OIS (SOFR-3M) nextExpiry December -> January rollover");
    setupOisFutureConvention("USD-SOFR-3M-FUTURE", "USD-SOFR", "3M");
    IrConventionBasedFutureExpiry calc("USD-SOFR-3M-FUTURE");

    // December 17 is after Dec end date (Dec 16) -> should roll to Jan 2027 (Jan 20)
    Date ref(17, Dec, 2026);
    Date expected(20, Jan, 2027);
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, ref, 0), expected);
}

// --- OIS Future (SOFR-1M, FirstDayOfMonth) tests ---
// getOiFutureStartEndDate with FirstDayOfMonth and 1M tenor:
//   endDate = calendar.adjust(Date(1, month, year) + 1M)
//   startDate = calendar.adjust(Date(1, month, year) + 1M - 1M)
// For Apr 2026: end = adjust(May 1) = May 1 (Fri), start = adjust(Apr 1) = Apr 1 (Wed)
// For May 2026: end = adjust(Jun 1) = Jun 1 (Mon), start = adjust(May 1) = May 1 (Fri)

BOOST_AUTO_TEST_CASE(testOisFirstDayOfMonthNextExpiry) {
    BOOST_TEST_MESSAGE("Testing OIS (SOFR-1M) with FirstDayOfMonth rule");
    auto conventions = QuantLib::ext::make_shared<Conventions>();
    auto conv =
        QuantLib::ext::make_shared<FutureConvention>("USD-SOFR-1M-FUTURE", "USD-SOFR", RateAveraging::Type::Compound,
                                                     FutureConvention::DateGenerationRule::FirstDayOfMonth, "US", "1M");
    conventions->add(conv);
    InstrumentConventions::instance().setConventions(conventions);
    IrConventionBasedFutureExpiry calc("USD-SOFR-1M-FUTURE");

    // April 14: next end date is May 1 (adjust(Apr 1 + 1M + 1M) = adjust(May 1) = May 1)
    Date ref(14, Apr, 2026);
    Date aprEnd(1, May, 2026); // end date for Apr contract
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, ref, 0), aprEnd);

    // May 2: after May 1 end -> next contract's end = Jun 1
    Date ref2(2, May, 2026);
    Date mayEnd(1, Jun, 2026); // end date for May contract
    BOOST_CHECK_EQUAL(calc.nextExpiry(true, ref2, 0), mayEnd);
}

// --- expiryToIrCurveDate with OIS convention ---

BOOST_AUTO_TEST_CASE(testExpiryToIrCurveDateOisFutureContinuation) {
    BOOST_TEST_MESSAGE("Testing expiryToIrCurveDate with OIS FutureContinuationExpiry");
    setupOisFutureConvention("USD-SOFR-3M-FUTURE", "USD-SOFR", "3M");
    IrConventionBasedFutureExpiry calc("USD-SOFR-3M-FUTURE");

    Date ref(14, Apr, 2026);
    Date aprEnd(15, Apr, 2026);
    Date mayEnd(20, May, 2026);
    Date junEnd(17, Jun, 2026);

    auto c1 = FutureContinuationExpiry(1);
    BOOST_CHECK_EQUAL(expiryToIrCurveDate(c1, ref, calc), aprEnd);

    auto c2 = FutureContinuationExpiry(2);
    BOOST_CHECK_EQUAL(expiryToIrCurveDate(c2, ref, calc), mayEnd);

    auto c3 = FutureContinuationExpiry(3);
    BOOST_CHECK_EQUAL(expiryToIrCurveDate(c3, ref, calc), junEnd);
}

// --- Round-trip: nextExpiry end dates must match getOiFutureStartEndDate ---

BOOST_AUTO_TEST_CASE(testOisNextExpiryMatchesGetOiFutureStartEndDate) {
    BOOST_TEST_MESSAGE("Testing OIS nextExpiry end dates match getOiFutureStartEndDate");
    setupOisFutureConvention("USD-SOFR-3M-FUTURE", "USD-SOFR", "3M");
    IrConventionBasedFutureExpiry calc("USD-SOFR-3M-FUTURE");

    Period tenor = 3 * Months;
    auto rule = FutureConvention::DateGenerationRule::IMM;
    Calendar calendar = WeekendsOnly();

    Date ref(14, Apr, 2026);

    // c1, c2, c3 via nextExpiry
    for (Natural offset = 0; offset < 6; ++offset) {
        Date endDate = calc.nextExpiry(true, ref, offset);
        // Call getOiFutureStartEndDate with the month/year of the computed end date
        auto [startDate, expectedEnd] = getOiFutureStartEndDate(endDate.month(), endDate.year(), tenor, rule, calendar);
        BOOST_TEST_MESSAGE("Offset " << offset << ": nextExpiry end date = " << endDate
                                     << ", getOiFutureStartEndDate end date = " << expectedEnd
                                     << ", start date = " << startDate << " (contract month/year: " << endDate.month()
                                     << "/" << endDate.year() << ")");
        BOOST_CHECK_EQUAL(endDate, expectedEnd);
    }
}

BOOST_AUTO_TEST_CASE(testOisFirstDayOfMonthNextExpiryMatchesGetOiFutureStartEndDate) {
    BOOST_TEST_MESSAGE("Testing OIS (FirstDayOfMonth) nextExpiry end dates match getOiFutureStartEndDate");
    auto conventions = QuantLib::ext::make_shared<Conventions>();
    Calendar usCal = UnitedStates(UnitedStates::GovernmentBond);
    auto conv =
        QuantLib::ext::make_shared<FutureConvention>("USD-SOFR-1M-FUTURE", "USD-SOFR", RateAveraging::Type::Compound,
                                                     FutureConvention::DateGenerationRule::FirstDayOfMonth, "US", "3M");
    conventions->add(conv);
    InstrumentConventions::instance().setConventions(conventions);
    IrConventionBasedFutureExpiry calc("USD-SOFR-1M-FUTURE");

    Period tenor = 3 * Months;
    auto rule = FutureConvention::DateGenerationRule::FirstDayOfMonth;

    Date ref(14, Apr, 2026);

    for (Natural offset = 0; offset < 6; ++offset) {
        Date endDate = calc.nextExpiry(true, ref, offset);
        // The end date from nextExpiry falls in the following month (e.g. Apr contract -> May 1 end)
        // so we need to find which contract month this end date belongs to:
        // endDate = calendar.adjust(Date(1, contractMonth, contractYear) + tenor)
        // For 1M tenor with FirstDayOfMonth, contractMonth = month before the endDate's month
        Month contractMonth = endDate.month() == Jan ? Dec : Month(endDate.month() - 1);
        Year contractYear = endDate.month() == Jan ? endDate.year() - 1 : endDate.year();
        auto [startDate, expectedEnd] = getOiFutureStartEndDate(contractMonth, contractYear, tenor, rule, usCal);
        BOOST_TEST_MESSAGE("Offset " << offset << ": nextExpiry end date = " << endDate
                                     << ", getOiFutureStartEndDate end date = " << expectedEnd
                                     << ", start date = " << startDate << " (contract month/year: " << contractMonth
                                     << "/" << contractYear << ")");
        BOOST_CHECK_EQUAL(endDate, expectedEnd);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

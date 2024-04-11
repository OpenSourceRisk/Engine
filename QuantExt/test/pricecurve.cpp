/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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
#include "toplevelfixture.hpp"
#include <boost/assign/list_of.hpp>
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>

#include <ql/currencies/america.hpp>
#include <ql/math/interpolations/all.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

#include <qle/termstructures/pricecurve.hpp>

using namespace std;

using namespace boost::unit_test_framework;
using namespace boost::assign;

using namespace QuantLib;
using namespace QuantExt;

namespace {

class CommonData {
public:
    // Variables
    vector<Date> testDates;
    vector<Period> curveTenors;
    vector<Real> prices;
    vector<Real> shiftedPrices;
    vector<QuantLib::ext::shared_ptr<SimpleQuote> > pQuotes;
    vector<Handle<Quote> > quotes;

    // Dates on which we will check interpolated values
    vector<Date> interpolationDates;
    vector<Time> interpolationTimes;
    // Expected (linear) interpolation results on first test date
    vector<Real> baseExpInterpResults;
    // Expected (linear) interpolation results for floating reference date
    // curve after moving to second test date and requesting price by date
    vector<Real> afterExpInterpResults;
    // Expected (linear) interpolation results on first test date after
    // shifting quotes
    vector<Real> baseNewInterpResults;
    // Expected (linear) interpolation results for floating reference date
    // curve after shifting quotes and moving to second test date and
    // requesting price by date
    vector<Real> afterNewInterpResults;

    // Expected loglinear interpolation results
    vector<Real> expLogInterpResults;

    DayCounter curveDayCounter;
    Real tolerance;
    bool extrapolate;
    USDCurrency currency;

    // cleanup
    SavedSettings backup;

    // Default constructor
    CommonData()
        : testDates(2), curveTenors(6), prices(6), shiftedPrices(6), pQuotes(6), quotes(6), interpolationDates(3),
          interpolationTimes(3), baseExpInterpResults(3), afterExpInterpResults(3), baseNewInterpResults(3),
          afterNewInterpResults(3), expLogInterpResults(3), curveDayCounter(Actual365Fixed()), tolerance(1e-10),
          extrapolate(true) {

        // Evaluation dates on which tests will be performed
        testDates[0] = Date(15, Feb, 2018);
        testDates[1] = Date(15, Mar, 2018);

        // Curve tenors and prices
        curveTenors[0] = 0 * Days;
        prices[0] = 14.5;
        shiftedPrices[0] = 16.6;
        curveTenors[1] = 181 * Days;
        prices[1] = 16.7;
        shiftedPrices[1] = 19.9;
        curveTenors[2] = 365 * Days;
        prices[2] = 19.9;
        shiftedPrices[2] = 24.4;
        curveTenors[3] = 546 * Days;
        prices[3] = 24.5;
        shiftedPrices[3] = 31.3;
        curveTenors[4] = 730 * Days;
        prices[4] = 28.5;
        shiftedPrices[4] = 38.1;
        curveTenors[5] = 1826 * Days;
        prices[5] = 38.8;
        shiftedPrices[5] = 54.5;

        // Create quotes
        for (Size i = 0; i < quotes.size(); i++) {
            pQuotes[i] = QuantLib::ext::make_shared<SimpleQuote>(prices[i]);
            quotes[i] = Handle<Quote>(pQuotes[i]);
        }

        // Interpolation variables
        interpolationDates[0] = Date(1, Jan, 2019);
        interpolationDates[1] = Date(1, Jun, 2021);
        interpolationDates[2] = Date(1, Aug, 2025);

        for (Size i = 0; i < interpolationDates.size(); i++) {
            interpolationTimes[i] = curveDayCounter.yearFraction(testDates[0], interpolationDates[i]);
        }

        baseExpInterpResults[0] = 19.1173913043478;
        baseExpInterpResults[1] = 32.9357664233577;
        baseExpInterpResults[2] = 47.2392335766423;

        afterExpInterpResults[0] = 18.6304347826087;
        afterExpInterpResults[1] = 32.6726277372263;
        afterExpInterpResults[2] = 46.9760948905109;

        baseNewInterpResults[0] = 23.2994565217391;
        baseNewInterpResults[1] = 45.1627737226277;
        baseNewInterpResults[2] = 67.9372262773723;

        afterNewInterpResults[0] = 22.6146739130435;
        afterNewInterpResults[1] = 44.7437956204380;
        afterNewInterpResults[2] = 67.5182481751825;

        expLogInterpResults[0] = 19.0648200765280;
        expLogInterpResults[1] = 32.5497181830507;
        expLogInterpResults[2] = 49.9589077461237;
    }

    // Give curve dates for one of the two test dates
    vector<Date> dates(Size testDatesIdx) const {
        vector<Date> result(curveTenors.size());
        for (Size i = 0; i < result.size(); i++) {
            result[i] = testDates[testDatesIdx] + curveTenors[i];
        }
        return result;
    }

    // Update quotes with new shifted prices
    void updateQuotes() const {
        for (Size i = 0; i < quotes.size(); i++) {
            pQuotes[i]->setValue(shiftedPrices[i]);
        }
    }
};

// Perform some common curve checks on the first test date
template <class I> void commonChecks(CommonData& td, InterpolatedPriceCurve<I>& priceCurve, bool isLogLinear = false) {
    BOOST_TEST_MESSAGE("Performing common curve checks");

    // Check the prices at the pillar dates
    for (Size i = 0; i < td.dates(0).size(); i++) {
        BOOST_CHECK_CLOSE(td.prices[i], priceCurve.price(td.dates(0)[i], td.extrapolate), td.tolerance);
    }

    // Check some interpolated & extrapolated values
    vector<Real> expResults = isLogLinear ? td.expLogInterpResults : td.baseExpInterpResults;

    for (Size i = 0; i < td.interpolationDates.size(); i++) {
        BOOST_CHECK_CLOSE(expResults[i], priceCurve.price(td.interpolationDates[i], td.extrapolate), td.tolerance);
        BOOST_CHECK_CLOSE(expResults[i], priceCurve.price(td.interpolationTimes[i], td.extrapolate), td.tolerance);
    }
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(PriceCurveTest)

BOOST_AUTO_TEST_CASE(testPeriodsAndPricesCurve) {

    BOOST_TEST_MESSAGE("Testing interpolated price curve built from periods and prices");

    CommonData td;

    // Look at the first test date
    Settings::instance().evaluationDate() = td.testDates[0];

    // Create a linearly interpolated price curve
    InterpolatedPriceCurve<Linear> priceCurve(td.curveTenors, td.prices, td.curveDayCounter, td.currency);

    // Common checks on curve
    commonChecks(td, priceCurve, false);

    // Create a loglinearly interpolated price curve
    InterpolatedPriceCurve<LogLinear> logPriceCurve(td.curveTenors, td.prices, td.curveDayCounter, td.currency);

    // Common checks on curve
    commonChecks(td, logPriceCurve, true);

    // Check linearly interpolated price curve after moving reference date
    Settings::instance().evaluationDate() = td.testDates[1];

    // Check curve reference date is now second test date
    BOOST_CHECK_EQUAL(priceCurve.referenceDate(), td.testDates[1]);

    // Requesting price by time should give the same results as previously
    // Requesting by date should give new results (floating reference date curve)
    for (Size i = 0; i < td.interpolationTimes.size(); i++) {
        BOOST_CHECK_CLOSE(td.baseExpInterpResults[i], priceCurve.price(td.interpolationTimes[i], td.extrapolate),
                          td.tolerance);
        BOOST_CHECK_CLOSE(td.afterExpInterpResults[i], priceCurve.price(td.interpolationDates[i], td.extrapolate),
                          td.tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testPeriodsAndQuotesCurve) {

    BOOST_TEST_MESSAGE("Testing interpolated price curve built from periods and quotes");

    CommonData td;

    // Look at the first test date
    Settings::instance().evaluationDate() = td.testDates[0];

    // Create a linearly interpolated price curve
    InterpolatedPriceCurve<Linear> priceCurve(td.curveTenors, td.quotes, td.curveDayCounter, td.currency);

    // Common checks on curve
    commonChecks(td, priceCurve, false);

    // Create a loglinearly interpolated price curve
    InterpolatedPriceCurve<LogLinear> logPriceCurve(td.curveTenors, td.quotes, td.curveDayCounter, td.currency);

    // Common checks on curve
    commonChecks(td, logPriceCurve, true);

    // Check linearly interpolated price curve after moving reference date
    Settings::instance().evaluationDate() = td.testDates[1];

    // Check curve reference date is now second test date
    BOOST_CHECK_EQUAL(priceCurve.referenceDate(), td.testDates[1]);

    // Requesting price by time should give the same results as previously
    // Requesting by date should give new results (floating reference date curve)
    for (Size i = 0; i < td.interpolationTimes.size(); i++) {
        BOOST_CHECK_CLOSE(td.baseExpInterpResults[i], priceCurve.price(td.interpolationTimes[i], td.extrapolate),
                          td.tolerance);
        BOOST_CHECK_CLOSE(td.afterExpInterpResults[i], priceCurve.price(td.interpolationDates[i], td.extrapolate),
                          td.tolerance);
    }

    // Update quotes and check interpolations again (on this second test date)
    td.updateQuotes();
    for (Size i = 0; i < td.interpolationTimes.size(); i++) {
        BOOST_CHECK_CLOSE(td.baseNewInterpResults[i], priceCurve.price(td.interpolationTimes[i], td.extrapolate),
                          td.tolerance);
        BOOST_CHECK_CLOSE(td.afterNewInterpResults[i], priceCurve.price(td.interpolationDates[i], td.extrapolate),
                          td.tolerance);
    }

    // Move date back to first test date
    Settings::instance().evaluationDate() = td.testDates[0];

    // Check interpolations again with the new quotes
    for (Size i = 0; i < td.interpolationTimes.size(); i++) {
        BOOST_CHECK_CLOSE(td.baseNewInterpResults[i], priceCurve.price(td.interpolationTimes[i], td.extrapolate),
                          td.tolerance);
        BOOST_CHECK_CLOSE(td.baseNewInterpResults[i], priceCurve.price(td.interpolationDates[i], td.extrapolate),
                          td.tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testDatesAndPricesCurve) {

    BOOST_TEST_MESSAGE("Testing interpolated price curve built from dates and prices");

    CommonData td;

    // Look at the first test date
    Settings::instance().evaluationDate() = td.testDates[0];

    // Create a linearly interpolated price curve
    vector<Date> dates = td.dates(0);
    InterpolatedPriceCurve<Linear> priceCurve(dates[0], dates, td.prices, td.curveDayCounter, td.currency);

    // Common checks on curve
    commonChecks(td, priceCurve, false);

    // Create a loglinearly interpolated price curve
    InterpolatedPriceCurve<LogLinear> logPriceCurve(dates[0], dates, td.prices, td.curveDayCounter, td.currency);

    // Common checks on curve
    commonChecks(td, logPriceCurve, true);

    // Check linearly interpolated price curve after moving reference date
    Settings::instance().evaluationDate() = td.testDates[1];

    // Check curve reference date is still first test date
    BOOST_CHECK_EQUAL(priceCurve.referenceDate(), td.testDates[0]);

    // Requesting price by time or date should give the same results as previously
    // because this is a fixed reference date curve
    for (Size i = 0; i < td.interpolationTimes.size(); i++) {
        BOOST_CHECK_CLOSE(td.baseExpInterpResults[i], priceCurve.price(td.interpolationTimes[i], td.extrapolate),
                          td.tolerance);
        BOOST_CHECK_CLOSE(td.baseExpInterpResults[i], priceCurve.price(td.interpolationDates[i], td.extrapolate),
                          td.tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testDatesAndQuotesCurve) {

    BOOST_TEST_MESSAGE("Testing interpolated price curve built from dates and quotes");

    CommonData td;

    // Look at the first test date
    Settings::instance().evaluationDate() = td.testDates[0];

    // Create a linearly interpolated price curve
    vector<Date> dates = td.dates(0);
    InterpolatedPriceCurve<Linear> priceCurve(dates[0], dates, td.quotes, td.curveDayCounter, td.currency);

    // Common checks on curve
    commonChecks(td, priceCurve, false);

    // Create a loglinearly interpolated price curve
    InterpolatedPriceCurve<LogLinear> logPriceCurve(dates[0], dates, td.quotes, td.curveDayCounter, td.currency);

    // Common checks on curve
    commonChecks(td, logPriceCurve, true);

    // Check linearly interpolated price curve after moving reference date
    Settings::instance().evaluationDate() = td.testDates[1];

    // Check curve reference date is still first test date
    BOOST_CHECK_EQUAL(priceCurve.referenceDate(), td.testDates[0]);

    // Requesting price by time or date should give the same results as previously
    // because this is a fixed reference date curve
    for (Size i = 0; i < td.interpolationTimes.size(); i++) {
        BOOST_CHECK_CLOSE(td.baseExpInterpResults[i], priceCurve.price(td.interpolationTimes[i], td.extrapolate),
                          td.tolerance);
        BOOST_CHECK_CLOSE(td.baseExpInterpResults[i], priceCurve.price(td.interpolationDates[i], td.extrapolate),
                          td.tolerance);
    }

    // Update quotes and check the new interpolated values
    td.updateQuotes();
    for (Size i = 0; i < td.interpolationTimes.size(); i++) {
        BOOST_CHECK_CLOSE(td.baseNewInterpResults[i], priceCurve.price(td.interpolationTimes[i], td.extrapolate),
                          td.tolerance);
        BOOST_CHECK_CLOSE(td.baseNewInterpResults[i], priceCurve.price(td.interpolationDates[i], td.extrapolate),
                          td.tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testNoTimeZeroWorks) {

    BOOST_TEST_MESSAGE("Test building with periods without a time 0 works with extrapolation on");

    CommonData td;

    // Look at the first test date
    Settings::instance().evaluationDate() = td.testDates[0];

    // Create the price curve after removing the time 0 pillar
    vector<Period> tenors = td.curveTenors;
    tenors.erase(tenors.begin());
    td.prices.erase(td.prices.begin());
    InterpolatedPriceCurve<Linear> priceCurve(tenors, td.prices, td.curveDayCounter, td.currency);

    // Check requests for prices between first curve time ~0.5 and 0
    BOOST_CHECK_CLOSE(15.1391304347826, priceCurve.price(0.25, td.extrapolate), td.tolerance);
    BOOST_CHECK_CLOSE(13.5521739130435, priceCurve.price(0.0, td.extrapolate), td.tolerance);

    // Test log-linear interpolation also
    InterpolatedPriceCurve<LogLinear> logPriceCurve(tenors, td.prices, td.curveDayCounter, td.currency);
    BOOST_CHECK_CLOSE(15.331307232214800, logPriceCurve.price(0.25, td.extrapolate), td.tolerance);
    BOOST_CHECK_CLOSE(14.054688467053400, logPriceCurve.price(0.0, td.extrapolate), td.tolerance);

    // Test that an exception is thrown when extrapolation is off
    BOOST_CHECK_THROW(priceCurve.price(0.25, false), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testNegativeTimeRequestThrows) {

    BOOST_TEST_MESSAGE("Test that requesting a price at a time before zero throws");

    CommonData td;

    // Look at the first test date
    Date today = td.testDates[0];
    Settings::instance().evaluationDate() = today;

    // Create the price curve
    InterpolatedPriceCurve<Linear> priceCurve(td.curveTenors, td.prices, td.curveDayCounter, td.currency);

    // Check requests for prices at times < 0
    Time t = -0.5;
    Date d = today - 1 * Weeks;
    BOOST_CHECK_THROW(priceCurve.price(t, td.extrapolate), QuantLib::Error);
    BOOST_CHECK_THROW(priceCurve.price(d, td.extrapolate), QuantLib::Error);

    // Update evaluation date and request on today should throw similarly
    Settings::instance().evaluationDate() = today + 1 * Weeks;
    BOOST_CHECK_THROW(priceCurve.price(today, td.extrapolate), QuantLib::Error);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include "blackvariancecurve.hpp"
#include <qle/termstructures/blackvariancecurve3.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <boost/make_shared.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;
using namespace std;

void BlackVarianceCurveTest::testBlackVarianceCurve() {

    BOOST_TEST_MESSAGE("Testing QuantExt::BlackVarianceCurve3...");

    SavedSettings backup;
    Settings::instance().evaluationDate() = Date(1, Dec, 2015);
    Date today = Settings::instance().evaluationDate();

    Natural settlementDays = 0;
    Calendar cal = TARGET();
    BusinessDayConvention bdc = Following;
    DayCounter dc = ActualActual();

    vector<Time> times;
    vector<Date> dates;
    vector<Volatility> vols;
    vector<boost::shared_ptr<SimpleQuote> > simpleQuotes;
    vector<Handle<Quote> > quotes;

    Size numYears = 10;
    for(Size i = 1; i < numYears; i++) {

        Volatility vol = 0.1 + (0.01*i); // 11% at 1Y, 12% at 2Y
        vols.push_back(vol);

        simpleQuotes.push_back(boost::make_shared<SimpleQuote>(vol));
        quotes.push_back(Handle<Quote>(simpleQuotes.back()));

        dates.push_back(Date(1, Dec, today.year()+i));
        times.push_back(dc.yearFraction(today, dates.back()));
    }


    // Build a QuantLib::BlackVarianceCurve
    BlackVarianceCurve bvcBase(today, dates, vols, dc);
    bvcBase.enableExtrapolation();

    // Build a QuantExt::BlackVarianceCurve3
    BlackVarianceCurve3 bvcTest(settlementDays, cal, bdc, dc, times, quotes);
    bvcTest.enableExtrapolation();

    Real strike = 1.0; // this is all ATM so we don't care

    // Check that bvcTest returns the expected values
    for (Size i = 0; i < times.size(); ++i) {
        BOOST_CHECK_CLOSE(bvcTest.blackVol(times[i], strike), vols[i], 1e-12);
        BOOST_CHECK_CLOSE(bvcTest.blackVol(dates[i], strike), vols[i], 1e-12);
    }

    // Now check that they give the same vols (including extrapolation)
    for (Time t = 0.1; t < numYears + 10.0; t += 0.1) {
        BOOST_CHECK_CLOSE(bvcBase.blackVol(t, strike), bvcTest.blackVol(t, strike), 1e-12);
    }

    // Now double the quotes
    for (Size i = 0; i < simpleQuotes.size(); ++i) {
        simpleQuotes[i]->setValue(simpleQuotes[i]->value() * 2.0);
    }
    // and check again
    for (Time t = 0.1; t < numYears + 10.0; t += 0.1) {
        BOOST_CHECK_CLOSE(bvcBase.blackVol(t, strike), 0.5 * bvcTest.blackVol(t, strike), 1e-12);
    }
}

test_suite* BlackVarianceCurveTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("BlackVarianceCurveTest");
    suite->add(BOOST_TEST_CASE(&BlackVarianceCurveTest::testBlackVarianceCurve));
    return suite;
}

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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
#include <boost/test/unit_test.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/termstructures/interpolateddiscountcurve2.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;
using std::vector;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(DiscountCurveTest)

BOOST_AUTO_TEST_CASE(testDiscountCurve) {

    // FIXME: test curve1 or 2 or both
    BOOST_TEST_MESSAGE("Testing QuantExt::InteroplatedDiscountCurve2...");

    SavedSettings backup;
    Settings::instance().evaluationDate() = Date(1, Dec, 2015);
    Date today = Settings::instance().evaluationDate();

    vector<Date> dates;
    vector<Real> times;
    vector<DiscountFactor> dfs;
    vector<Handle<Quote> > quotes;

    Size numYears = 30;
    int startYear = 2015;
    DayCounter dc = ActualActual(ActualActual::ISDA);
    Calendar cal = NullCalendar();

    for (Size i = 0; i < numYears; i++) {

        // rate
        Real rate = 0.01 + i * 0.001;
        // 1 year apart
        dates.push_back(Date(1, Dec, startYear + i));
        Time t = dc.yearFraction(today, dates.back());
        times.push_back(t);

        // set up Quote of DiscountFactors
        DiscountFactor df = ::exp(-rate * t);
        Handle<Quote> q(QuantLib::ext::make_shared<SimpleQuote>(df));
        quotes.push_back(q);
        dfs.push_back(df);
    }

    // Test against the QL curve
    QuantLib::ext::shared_ptr<YieldTermStructure> ytsBase;
    ytsBase =
        QuantLib::ext::shared_ptr<YieldTermStructure>(new QuantLib::InterpolatedDiscountCurve<LogLinear>(dates, dfs, dc, cal));
    ytsBase->enableExtrapolation();

    QuantLib::ext::shared_ptr<YieldTermStructure> ytsTest(new QuantExt::InterpolatedDiscountCurve2(times, quotes, dc));

    // now check that they give the same discount factors (including extrapolation)
    for (Time t = 0.1; t < numYears + 10.0; t += 0.1) {
        BOOST_CHECK_CLOSE(ytsBase->discount(t), ytsTest->discount(t), 1e-12);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

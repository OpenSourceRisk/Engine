/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2015 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include "discountcurve.hpp"
#include <qle/termstructures/interpolateddiscountcurve2.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace boost::unit_test_framework;
using std::vector;

void DiscountCurveTest::testDiscountCurve() {

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
    DayCounter dc = ActualActual();
    Calendar cal = NullCalendar();

    for(Size i = 0; i < numYears; i++) {

        // rate
        Real rate = 0.01 + i*0.001;
        // 1 year apart
        dates.push_back(Date(1, Dec, startYear+i));
        Time t = dc.yearFraction(today,dates.back());
        times.push_back(t);

        // set up Quote of DiscountFactors
        DiscountFactor df = ::exp(-rate*t);
        Handle<Quote> q (boost::make_shared<SimpleQuote>(df));
        quotes.push_back(q);
        dfs.push_back(df);
    }

    // Test against the QL curve
    boost::shared_ptr<YieldTermStructure> ytsBase;
    ytsBase = boost::shared_ptr<YieldTermStructure>(
        new QuantLib::InterpolatedDiscountCurve<LogLinear>(dates, dfs, dc,
                                                           cal));
    ytsBase->enableExtrapolation();

    boost::shared_ptr<YieldTermStructure> ytsTest
        (new QuantExt::InterpolatedDiscountCurve2(times, quotes, dc));

    // now check that they give the same discount factors (including extrapolation)
    for (Time t = 0.1; t < numYears + 10.0; t += 0.1) {
        BOOST_CHECK_CLOSE(ytsBase->discount(t), ytsTest->discount(t), 1e-12);
    }
}

test_suite* DiscountCurveTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("DiscountCurveTests");
    suite->add(BOOST_TEST_CASE(&DiscountCurveTest::testDiscountCurve));
    return suite;
}

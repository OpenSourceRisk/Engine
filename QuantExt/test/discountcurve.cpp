/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2015 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include "discountcurve.hpp"
#include <qle/termstructures/interpolateddiscountcurve.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace boost::unit_test_framework;
using std::vector;

void test(bool logLinear) {

    Date today = Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = Date(1, Dec, 2015);

    vector<Date> dates;
    vector<DiscountFactor> dfs;
    vector<Handle<Quote> > quotes;

    Size numYears = 30;
    int startYear = 2015;
    for(Size i = 0; i < numYears; i++) {

        // rate
        Real rate = 0.01 + i*0.001;
        // 1 year apart
        dates.push_back(Date(1, Dec, startYear+i));
        Time t = i*1.0;

        // set up Quote of DiscountFactors
        DiscountFactor df = ::exp(-rate*t);
        Handle<Quote> q (boost::make_shared<SimpleQuote>(df));
        quotes.push_back(q);
        dfs.push_back(df);
    }

    DayCounter dc = ActualActual();
    Calendar cal = TARGET();

    // Test against the QL curve
    boost::shared_ptr<YieldTermStructure> ytsBase;
    if(logLinear)
        ytsBase = boost::shared_ptr<YieldTermStructure>(new
            QuantLib::InterpolatedDiscountCurve<LogLinear>(dates, dfs, dc, cal));
    else
        ytsBase = boost::shared_ptr<YieldTermStructure>(new
            QuantLib::InterpolatedDiscountCurve<Linear>(dates, dfs, dc, cal));

    boost::shared_ptr<YieldTermStructure> ytsTest
        (new QuantExt::InterpolatedDiscountCurve(dates, quotes, cal, dc, logLinear));

    // now check that they give the same discount factors (up to extrapolation)
    for (Time t = 0.1; t < numYears - 1.0; t += 0.1)
        BOOST_CHECK_CLOSE(ytsBase->discount(t), ytsTest->discount(t), 1e-12);

    Settings::instance().evaluationDate() = today;
}

void DiscountCurveTest::testDiscountCurve() {
    BOOST_MESSAGE("Testing QuantExt::InteroplatedDiscountCurve (Linear) ...");
    test(false); //test linear interpolation
    BOOST_MESSAGE("Testing QuantExt::InteroplatedDiscountCurve (LogLinear) ...");
    test(true);  //test log linear interpolation
}

test_suite* DiscountCurveTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("DiscountCurveTests");
    suite->add(BOOST_TEST_CASE(&DiscountCurveTest::testDiscountCurve));
    return suite;
}

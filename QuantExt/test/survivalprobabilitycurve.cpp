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

#include "survivalprobabilitycurve.hpp"
#include <boost/test/unit_test.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/interpolatedsurvivalprobabilitycurve.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/termstructures/survivalprobabilitycurve.hpp>

using namespace boost::unit_test_framework;
using std::vector;

namespace testsuite {

void SurvivalProbabilityCurveTest::testSurvivalProbabilityCurve() {

    BOOST_TEST_MESSAGE("Testing QuantExt::SurvivalProbabilityCurve...");

    Settings::instance().evaluationDate() = Date(1, Dec, 2015);
    Date today = Settings::instance().evaluationDate();

    vector<Date> dates;
    vector<Real> times;
    vector<Probability> sps;
    vector<Handle<Quote> > quotes;
    vector<boost::shared_ptr<SimpleQuote> > simpleQuotes;

    Size numYears = 30;
    int startYear = 2015;
    DayCounter dc = ActualActual();
    Calendar cal = NullCalendar();

    for (Size i = 0; i < numYears; i++) {

        // rate
        Real rate = 0.01 + i * 0.001;
        // 1 year apart
        dates.push_back(Date(1, Dec, startYear + i));
        Time t = dc.yearFraction(today, dates.back());
        times.push_back(t);

        // set up Quote of SurvivalProbabilities
        Probability sp = ::exp(-rate * t);

        boost::shared_ptr<SimpleQuote> q(new SimpleQuote(sp));
        simpleQuotes.push_back(q);
        Handle<Quote> qh(q);
        quotes.push_back(qh);
        sps.push_back(sp);
    }

    // Test against the QL curve
    boost::shared_ptr<DefaultProbabilityTermStructure> dtsBase;
    dtsBase = boost::shared_ptr<DefaultProbabilityTermStructure>(
        new QuantLib::InterpolatedSurvivalProbabilityCurve<Linear>(dates, sps, dc, cal));
    dtsBase->enableExtrapolation();

    boost::shared_ptr<DefaultProbabilityTermStructure> dtsTest(
        new QuantExt::SurvivalProbabilityCurve<Linear>(dates, quotes, dc, cal));

    // now check that they give the same survivalProbabilities (including extrapolation)
    for (Time t = 0.1; t < numYears + 10.0; t += 0.1) {
        BOOST_CHECK_CLOSE(dtsBase->survivalProbability(t, true), dtsTest->survivalProbability(t, true), 1e-12);
    }

    for (Size i = 0; i < numYears; i++) {
        Probability sp = sps[i] + 0.1;
        simpleQuotes[i]->setValue(sp);
    }
    // now check that they no longer match
    for (Time t = 0.1; t < numYears + 10.0; t += 0.1) {
        BOOST_CHECK_PREDICATE(std::not_equal_to<Probability>(),
                              (dtsBase->survivalProbability(t, true))(dtsTest->survivalProbability(t, true)));
    }
}

test_suite* SurvivalProbabilityCurveTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("SurvivalProbabilityCurveTests");
    suite->add(BOOST_TEST_CASE(&SurvivalProbabilityCurveTest::testSurvivalProbabilityCurve));
    return suite;
}
} // namespace testsuite

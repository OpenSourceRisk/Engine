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
#include <qle/termstructures/flatcorrelation.hpp>
#include <qle/termstructures/interpolatedcorrelationcurve.hpp>

#include <ql/quotes/simplequote.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

#include <boost/make_shared.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CorrelationTermstructureTest)

BOOST_AUTO_TEST_CASE(testFlatCorrelation) {

    QuantLib::ext::shared_ptr<SimpleQuote> q = QuantLib::ext::make_shared<SimpleQuote>(0.02);
    Handle<Quote> hq(q);
    Handle<FlatCorrelation> flatCorr(QuantLib::ext::make_shared<FlatCorrelation>(0, NullCalendar(), hq, Actual365Fixed()));

    // check we get the expected quote value
    BOOST_CHECK_MESSAGE(flatCorr->correlation(1) == 0.02, "unexpected correlation value: " << flatCorr->correlation(1));

    // move market data
    q->setValue(0.03);

    BOOST_CHECK_MESSAGE(flatCorr->correlation(1) == 0.03, "unexpected correlation value: " << flatCorr->correlation(1));

    // check failures

    q->setValue(-1.1);
    BOOST_CHECK_THROW(flatCorr->correlation(1), QuantLib::Error);

    q->setValue(1.1);
    BOOST_CHECK_THROW(flatCorr->correlation(1), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testInterpolatedCorrelationCurve) {

    // build interpolated correlation curve
    std::vector<Time> times;
    std::vector<QuantLib::ext::shared_ptr<SimpleQuote> > simpleQuotes;
    std::vector<Handle<Quote> > quotes;

    Size numYears = 10;
    for (Size i = 1; i < numYears; i++) {

        Real corr = 0.1;

        simpleQuotes.push_back(QuantLib::ext::make_shared<SimpleQuote>(corr));
        quotes.push_back(Handle<Quote>(simpleQuotes.back()));

        times.push_back(static_cast<Time>(i));
    }

    Handle<PiecewiseLinearCorrelationCurve> interpCorr(
        QuantLib::ext::make_shared<PiecewiseLinearCorrelationCurve>(times, quotes, Actual365Fixed(), NullCalendar()));

    Time t = 1;
    while (t < numYears) {
        BOOST_CHECK_MESSAGE(interpCorr->correlation(t) == 0.1,
                            "unexpected correlation value: " << interpCorr->correlation(t));
        t += 0.5;
    }

    // Now check quotes update
    for (Size i = 0; i < simpleQuotes.size(); ++i) {
        simpleQuotes[i]->setValue(1);
    }

    t = 1;
    while (t < numYears) {
        BOOST_CHECK_MESSAGE(interpCorr->correlation(t) == 1,
                            "unexpected correlation value: " << interpCorr->correlation(t));
        t += 0.5;
    }

    // Now check interpolation
    for (Size i = 0; i < simpleQuotes.size(); ++i) {
        simpleQuotes[i]->setValue(0.1 + 0.01 * i);
    }

    Real tol = 1.0E-8;
    BOOST_CHECK_CLOSE(interpCorr->correlation(1.5), 0.105, tol);
    BOOST_CHECK_CLOSE(interpCorr->correlation(2.5), 0.115, tol);
    BOOST_CHECK_CLOSE(interpCorr->correlation(3.5), 0.125, tol);
    BOOST_CHECK_CLOSE(interpCorr->correlation(11), 0.18, tol);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

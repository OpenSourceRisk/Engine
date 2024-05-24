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
#include <ql/indexes/ibor/usdlibor.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>

#include <qle/termstructures/tenorbasisswaphelper.hpp>

#include <boost/make_shared.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(RateHelpersTest)

BOOST_AUTO_TEST_CASE(testTenorBasisSwapHelperLastRelevantDate) {

    BOOST_TEST_MESSAGE("Testing QuantExt::TenorBasisSwapHelper last relevant date (regression test case)...");

    SavedSettings backup;
    Settings::instance().evaluationDate() = Date(8, Dec, 2016);
    Date today = Settings::instance().evaluationDate();

    Handle<YieldTermStructure> flat6m(
        QuantLib::ext::make_shared<FlatForward>(today, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.02)), Actual365Fixed()));
    QuantLib::ext::shared_ptr<IborIndex> usdLibor6m = QuantLib::ext::make_shared<USDLibor>(6 * Months, flat6m);
    QuantLib::ext::shared_ptr<IborIndex> usdLibor1m = QuantLib::ext::make_shared<USDLibor>(1 * Months);

    QuantLib::ext::shared_ptr<RateHelper> helper = QuantLib::ext::make_shared<QuantExt::TenorBasisSwapHelper>(
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0)), 30 * Years, usdLibor6m, usdLibor1m, Handle<YieldTermStructure>(), true, false, 6 * Months);

    PiecewiseYieldCurve<Discount, LogLinear> curve(today, std::vector<QuantLib::ext::shared_ptr<RateHelper> >(1, helper),
                                                   Actual365Fixed());
    BOOST_CHECK_NO_THROW(curve.discount(1.0));
}

BOOST_AUTO_TEST_CASE(testTenorBasisSwapHelperDegenerateSchedule) {

    BOOST_TEST_MESSAGE("Testing QuantExt::TenorBasisSwapHelper degenerate schedule (regression test case)...");

    SavedSettings backup;
    Settings::instance().evaluationDate() = Date(28, Dec, 2016);
    Date today = Settings::instance().evaluationDate();

    Handle<YieldTermStructure> flat6m(
        QuantLib::ext::make_shared<FlatForward>(today, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.02)), Actual365Fixed()));
    QuantLib::ext::shared_ptr<IborIndex> usdLibor6m = QuantLib::ext::make_shared<USDLibor>(6 * Months, flat6m);
    QuantLib::ext::shared_ptr<IborIndex> usdLibor3m = QuantLib::ext::make_shared<USDLibor>(3 * Months);

    QuantLib::ext::shared_ptr<RateHelper> helper = QuantLib::ext::make_shared<QuantExt::TenorBasisSwapHelper>(
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0)), 18 * Months, usdLibor6m, usdLibor3m, Handle<YieldTermStructure>(), true, false, 6 * Months);

    PiecewiseYieldCurve<Discount, LogLinear> curve(today, std::vector<QuantLib::ext::shared_ptr<RateHelper> >(1, helper),
                                                   Actual365Fixed());
    BOOST_CHECK_NO_THROW(curve.discount(1.0));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

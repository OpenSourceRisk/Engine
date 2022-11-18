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
#include <boost/test/unit_test.hpp>
#include <ql/currencies/america.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/settings.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <qle/instruments/commodityspreadoption.hpp>
#include <qle/pricingengines/commodityspreadoptionengine.hpp>
#include <qle/termstructures/pricecurve.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;

namespace {} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CommoditySpreadOptionTests)

BOOST_AUTO_TEST_CASE(testCrossAssetFutureSpread) {
    Date today(5, Nov, 2022);
    Settings::instance().evaluationDate() = today;

    std::vector<Date> futureExpiryDates = {Date(5, Nov, 2022), Date(30, Nov, 2022), Date(31, Dec, 2022)};
    std::vector<Real> brentQuotes = {100., 105., 106.};
    std::vector<Real> wtiQuotes = {99., 104., 107.};

    auto brentCurve = Handle<PriceTermStructure>(boost::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, brentQuotes, Actual365Fixed(), USDCurrency()));
    auto wtiCurve = Handle<PriceTermStructure>(boost::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, wtiQuotes, Actual365Fixed(), USDCurrency()));

    auto discount = Handle<YieldTermStructure>(
        boost::make_shared<FlatForward>(today, Handle<Quote>(boost::make_shared<SimpleQuote>(0.03)), Actual365Fixed()));

    auto vol1 = Handle<BlackVolTermStructure>(
        boost::make_shared<BlackConstantVol>(today, NullCalendar(), 0.3, Actual365Fixed()));
    auto vol2 = Handle<BlackVolTermStructure>(
        boost::make_shared<BlackConstantVol>(today, NullCalendar(), 0.35, Actual365Fixed()));

    auto index1 =
        boost::make_shared<CommodityFuturesIndex>("BRENT_USD", Date(31, Dec, 2022), NullCalendar(), brentCurve);

    auto index2 = boost::make_shared<CommodityFuturesIndex>("WTI_USD", Date(31, Dec, 2022), NullCalendar(), wtiCurve);

    auto flow1 = boost::make_shared<CommodityIndexedCashFlow>(100, Date(31, Dec, 2022), Date(15, Dec, 2022), index1);

    auto flow2 = boost::make_shared<CommodityIndexedCashFlow>(100, Date(31, Dec, 2022), Date(15, Dec, 2022), index2);

    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(15, Dec, 2022));

    CommoditySpreadOption spreadOption(flow1, flow2, exercise, 1000., 2, Option::Call);

    auto engine = boost::make_shared<CommoditySpreadOptionAnalyticalEngine>(discount, vol1, vol2, 0);

    spreadOption.setPricingEngine(engine);

    std::cout << spreadOption.NPV() << std::endl;
}


BOOST_AUTO_TEST_CASE(testCalendarSpread) {
    Date today(5, Nov, 2022);
    Settings::instance().evaluationDate() = today;

    std::vector<Date> futureExpiryDates = {Date(5, Nov, 2022), Date(30, Nov, 2022), Date(31, Dec, 2022)};
    std::vector<Real> brentQuotes = {100., 101.37928862723487, 102.2159767886142};

    auto brentCurve = Handle<PriceTermStructure>(boost::make_shared<InterpolatedPriceCurve<Linear>>(
        today, futureExpiryDates, brentQuotes, Actual365Fixed(), USDCurrency()));

    auto discount = Handle<YieldTermStructure>(
        boost::make_shared<FlatForward>(today, Handle<Quote>(boost::make_shared<SimpleQuote>(0.03)), Actual365Fixed()));

    auto vol1 = Handle<BlackVolTermStructure>(
        boost::make_shared<BlackConstantVol>(today, NullCalendar(), 0.3, Actual365Fixed()));

    auto index1 =
        boost::make_shared<CommodityFuturesIndex>("BRENT_DEC_USD", Date(31, Dec, 2022), NullCalendar(), brentCurve);
    
    auto index2 =
        boost::make_shared<CommodityFuturesIndex>("BRENT_NOV_USD", Date(30, Nov, 2022), NullCalendar(), brentCurve);

    auto flow1 = boost::make_shared<CommodityIndexedCashFlow>(100, Date(31, Dec, 2022), Date(15, Nov, 2022), index1);

    auto flow2 = boost::make_shared<CommodityIndexedCashFlow>(100, Date(30, Nov, 2022), Date(15, Nov, 2022), index2);

    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(15, Nov, 2022));

    CommoditySpreadOption spreadOption(flow1, flow2, exercise, 1000., 1, Option::Call);

    auto engine = boost::make_shared<CommoditySpreadOptionAnalyticalEngine>(discount, vol1, vol1, 0.99);

    spreadOption.setPricingEngine(engine);

    std::cout << spreadOption.NPV() << std::endl;
}



BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

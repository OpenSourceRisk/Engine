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
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <ql/currencies/america.hpp>
#include <ql/settings.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>

#include <qle/pricingengines/discountingcommodityforwardengine.hpp>
#include <qle/termstructures/pricecurve.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(DiscountingCommodityForwardEngineTest)

BOOST_AUTO_TEST_CASE(testPricing) {

    BOOST_TEST_MESSAGE("Testing discounting commodity forward engine pricing");

    SavedSettings backup;

    // Tolerance for npv comparison
    Real tolerance = 1e-10;

    // Commodity forward base data
    Date asof(19, Feb, 2018);
    string name("GOLD_USD");
    USDCurrency currency;
    Date maturity(19, Feb, 2019);

    // Day counter for time
    Actual365Fixed dayCounter;

    // Discount curve
    vector<Date> dates(2);
    vector<DiscountFactor> dfs(2);
    dates[0] = asof;
    dfs[0] = 1.0;
    dates[1] = maturity;
    dfs[1] = 0.85;
    Handle<YieldTermStructure> discountCurve(QuantLib::ext::make_shared<InterpolatedDiscountCurve<LogLinear> >(
        InterpolatedDiscountCurve<LogLinear>(dates, dfs, dayCounter)));

    // Commodity Price curve
    vector<Real> prices(2);
    prices[0] = 1346.0;
    prices[1] = 1384.0;
    Handle<PriceTermStructure> priceCurve(
        QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear> >(asof, dates, prices, dayCounter, USDCurrency()));

    // Create the engine
    QuantLib::ext::shared_ptr<DiscountingCommodityForwardEngine> engine =
        QuantLib::ext::make_shared<DiscountingCommodityForwardEngine>(discountCurve);

    // Set evaluation date
    Settings::instance().evaluationDate() = asof;

    // Long 100 units with strike 1380.0
    QuantLib::ext::shared_ptr<CommodityIndex> index = QuantLib::ext::make_shared<CommoditySpotIndex>(
        name, NullCalendar(), priceCurve);
    Position::Type position = Position::Long;
    Real quantity = 100;
    Real strike = 1380.0;
    QuantLib::ext::shared_ptr<CommodityForward> forward =
        QuantLib::ext::make_shared<CommodityForward>(index, currency, position, quantity, maturity, strike);
    forward->setPricingEngine(engine);
    BOOST_CHECK_CLOSE(forward->NPV(), quantity * (prices[1] - strike) * dfs[1], tolerance);

    // Short 100 units with strike 1380.0
    position = Position::Short;
    forward = QuantLib::ext::make_shared<CommodityForward>(index, currency, position, quantity, maturity, strike);
    forward->setPricingEngine(engine);
    BOOST_CHECK_CLOSE(forward->NPV(), -quantity * (prices[1] - strike) * dfs[1], tolerance);

    // Short 50 units with strike 1380.0
    quantity = 50.0;
    forward = QuantLib::ext::make_shared<CommodityForward>(index, currency, position, quantity, maturity, strike);
    forward->setPricingEngine(engine);
    BOOST_CHECK_CLOSE(forward->NPV(), -quantity * (prices[1] - strike) * dfs[1], tolerance);

    // Short 50 units with strike 1375.0
    strike = 1375.0;
    forward = QuantLib::ext::make_shared<CommodityForward>(index, currency, position, quantity, maturity, strike);
    forward->setPricingEngine(engine);
    BOOST_CHECK_CLOSE(forward->NPV(), -quantity * (prices[1] - strike) * dfs[1], tolerance);

    // Bring maturity of forward in 6 months
    maturity = Date(19, Aug, 2018);
    forward = QuantLib::ext::make_shared<CommodityForward>(index, currency, position, quantity, maturity, strike);
    forward->setPricingEngine(engine);
    BOOST_CHECK_CLOSE(forward->NPV(),
                      -quantity * (priceCurve->price(maturity) - strike) * discountCurve->discount(maturity),
                      tolerance);

    // Make maturity of forward equal to today
    forward = QuantLib::ext::make_shared<CommodityForward>(index, currency, position, quantity, asof, strike);
    // includeReferenceDateEvents_ of Settings is false by default => value should equal 0
    forward->setPricingEngine(engine);
    BOOST_CHECK_CLOSE(forward->NPV(), 0.0, tolerance);
    // Set includeReferenceDateEvents_ of Settings to true => value should be today's price - strike
    Settings::instance().includeReferenceDateEvents() = true;
    forward->recalculate();
    BOOST_CHECK_CLOSE(forward->NPV(), -quantity * (prices[0] - strike), tolerance);
    // Override behaviour in engine i.e. don't include flows on reference date even when
    // Settings::instance().includeReferenceDateEvents() is true
    engine = QuantLib::ext::make_shared<DiscountingCommodityForwardEngine>(discountCurve, false);
    forward->setPricingEngine(engine);
    BOOST_CHECK_CLOSE(forward->NPV(), 0.0, tolerance);
    // Trying the other way around should not work as the trade is marked as expired
    Settings::instance().includeReferenceDateEvents() = false;
    engine = QuantLib::ext::make_shared<DiscountingCommodityForwardEngine>(discountCurve, true);
    forward->setPricingEngine(engine);
    BOOST_CHECK_CLOSE(forward->NPV(), 0.0, tolerance);

    // Reinstate original maturity and change the npv date in the engine to 2 days after asof
    maturity = Date(19, Feb, 2019);
    forward = QuantLib::ext::make_shared<CommodityForward>(index, currency, position, quantity, maturity, strike);
    Date npvDate = asof + 2 * Days;
    engine = QuantLib::ext::make_shared<DiscountingCommodityForwardEngine>(discountCurve, boost::none, npvDate);
    forward->setPricingEngine(engine);
    DiscountFactor npvDateDiscount = discountCurve->discount(npvDate);
    BOOST_CHECK_CLOSE(forward->NPV(), -quantity * (prices[1] - strike) * dfs[1] / npvDateDiscount, tolerance);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

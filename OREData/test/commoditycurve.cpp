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

#include <boost/test/unit_test.hpp>
#include <oret/toplevelfixture.hpp>
#include <boost/make_shared.hpp>

#include <ql/time/daycounters/actualactual.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/commoditycurve.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;

namespace {

// testTolerance for Real comparison
Real testTolerance = 1e-10;

class MockLoader : public Loader {
public:
    MockLoader();
    const vector<boost::shared_ptr<MarketDatum>>& loadQuotes(const Date&) const { return data_; }
    const boost::shared_ptr<MarketDatum>& get(const string& name, const Date&) const { return dummyDatum_; }
    const vector<Fixing>& loadFixings() const { return dummyFixings_; }

private:
    vector<boost::shared_ptr<MarketDatum>> data_;
    boost::shared_ptr<MarketDatum> dummyDatum_;
    vector<Fixing> dummyFixings_;
};

MockLoader::MockLoader() {
    Date asof(5, Feb, 2016);
    data_ = {
        boost::make_shared<CommoditySpotQuote>(1155.593, asof, "COMMODITY/PRICE/GOLD/USD", MarketDatum::QuoteType::PRICE, "GOLD", "USD"), 
        boost::make_shared<CommodityForwardQuote>(1157.8, asof, "COMMODITY_FWD/PRICE/GOLD/USD/2016-02-29", MarketDatum::QuoteType::PRICE, "GOLD", "USD", Date(29, Feb, 2016)),
        boost::make_shared<CommodityForwardQuote>(1160.9, asof, "COMMODITY_FWD/PRICE/GOLD/USD/2017-02-28", MarketDatum::QuoteType::PRICE, "GOLD", "USD", Date(28, Feb, 2017)),
        boost::make_shared<CommodityForwardQuote>(1189.5, asof, "COMMODITY_FWD/PRICE/GOLD/USD/2020-06-30", MarketDatum::QuoteType::PRICE, "GOLD", "USD", Date(30, Jun, 2020))
    };
}

}

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CommodityCurveTests)

BOOST_AUTO_TEST_CASE(testCommodityCurveConstruction) {

    BOOST_TEST_MESSAGE("Testing commodity curve building");

    // As of date
    Date asof(5, Feb, 2016);

    // Commodity curve configuration
    vector<string> quotes = {
        "COMMODITY_FWD/PRICE/GOLD/USD/2016-02-29", 
        "COMMODITY_FWD/PRICE/GOLD/USD/2017-02-28", 
        "COMMODITY_FWD/PRICE/GOLD/USD/2020-06-30"
    };
    boost::shared_ptr<CommodityCurveConfig> curveConfig = boost::make_shared<CommodityCurveConfig>(
        "GOLD_USD", "", "USD", "COMMODITY/PRICE/GOLD/USD", quotes, "A365", "Linear", true);

    // Curve configurations
    CurveConfigurations curveConfigs;
    curveConfigs.commodityCurveConfig("GOLD_USD") = curveConfig;

    // Commodity curve spec
    CommodityCurveSpec curveSpec("USD", "GOLD_USD");

    // Market data loader
    MockLoader loader;

    // Check commodity curve construction works
    boost::shared_ptr<CommodityCurve> curve;
    BOOST_CHECK_NO_THROW(curve = boost::make_shared<CommodityCurve>(asof, curveSpec, loader, curveConfigs, Conventions()));

    // Check a few prices
    boost::shared_ptr<PriceTermStructure> priceCurve = curve->commodityPriceCurve();

    // Check a few prices
    BOOST_CHECK_CLOSE(priceCurve->price(asof), 1155.593, testTolerance);
    BOOST_CHECK_CLOSE(priceCurve->price(Date(29, Feb, 2016)), 1157.8, testTolerance);
    BOOST_CHECK_CLOSE(priceCurve->price(Date(28, Feb, 2017)), 1160.9, testTolerance);
    BOOST_CHECK_CLOSE(priceCurve->price(Date(30, Jun, 2020)), 1189.5, testTolerance);

    // Will use this to check the interpolation on the price curve via a dynamic cast
    boost::shared_ptr<PriceTermStructure> checkCurveInterpolation;

    // Check that variables were picked up from the config
    BOOST_CHECK_EQUAL(priceCurve->dayCounter(), Actual365Fixed());
    BOOST_CHECK_EQUAL(priceCurve->allowsExtrapolation(), true);
    checkCurveInterpolation = boost::dynamic_pointer_cast<InterpolatedPriceCurve<Linear>>(priceCurve);
    BOOST_CHECK(checkCurveInterpolation);

    // Change the config and check that the variables again
    curveConfig = boost::make_shared<CommodityCurveConfig>(
        "GOLD_USD", "", "USD", "COMMODITY/PRICE/GOLD/USD", quotes, "ACT/ACT", "LogLinear", false);
    curveConfigs.commodityCurveConfig("GOLD_USD") = curveConfig;

    BOOST_CHECK_NO_THROW(curve = boost::make_shared<CommodityCurve>(asof, curveSpec, loader, curveConfigs, Conventions()));
    priceCurve = curve->commodityPriceCurve();
    BOOST_CHECK_EQUAL(priceCurve->dayCounter(), ActualActual());
    BOOST_CHECK_EQUAL(priceCurve->allowsExtrapolation(), false);
    checkCurveInterpolation = boost::dynamic_pointer_cast<InterpolatedPriceCurve<LogLinear>>(priceCurve);
    BOOST_CHECK(checkCurveInterpolation);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

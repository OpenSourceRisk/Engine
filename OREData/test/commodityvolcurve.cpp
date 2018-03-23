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

#include <test/commodityvolcurve.hpp>

#include <boost/make_shared.hpp>

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/commodityvolcurve.hpp>
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
        boost::make_shared<CommodityOptionQuote>(0.11, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/1Y/ATMF", MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", "1Y", "ATMF"),
        boost::make_shared<CommodityOptionQuote>(0.10, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/2Y/ATMF", MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", "2Y", "ATMF"),
        boost::make_shared<CommodityOptionQuote>(0.09, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/5Y/ATMF", MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", "5Y", "ATMF"),
        boost::make_shared<CommodityOptionQuote>(0.105, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD_USD_VOLS/USD/1Y/1150", MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", "1Y", "1150"),
        boost::make_shared<CommodityOptionQuote>(0.115, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD_USD_VOLS/USD/1Y/1190", MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", "1Y", "1190"),
        boost::make_shared<CommodityOptionQuote>(0.095, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD_USD_VOLS/USD/2Y/1150", MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", "2Y", "1150"),
        boost::make_shared<CommodityOptionQuote>(0.105, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD_USD_VOLS/USD/2Y/1190", MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", "2Y", "1190"),
        boost::make_shared<CommodityOptionQuote>(0.085, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD_USD_VOLS/USD/5Y/1150", MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", "5Y", "1150"),
        boost::make_shared<CommodityOptionQuote>(0.095, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD_USD_VOLS/USD/5Y/1190", MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", "5Y", "1190")
    };
}

}

namespace testsuite {

void CommodityVolCurveTest::testCommodityVolCurveTypeConstant() {

    BOOST_TEST_MESSAGE("Testing commodity vol curve building with a single configured volatility");

    // Ensure settings are restored after this test
    SavedSettings backup;

    // As of date
    Date asof(5, Feb, 2016);

    // Volatility configuration with a single quote
    boost::shared_ptr<CommodityVolatilityCurveConfig> curveConfig = boost::make_shared<CommodityVolatilityCurveConfig>(
        "GOLD_USD_VOLS", "", "USD", "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/2Y/ATMF", "A365", "NullCalendar");

    // Curve configurations
    CurveConfigurations curveConfigs;
    curveConfigs.commodityVolatilityCurveConfig("GOLD_USD_VOLS") = curveConfig;

    // Commodity curve spec
    CommodityVolatilityCurveSpec curveSpec("USD", "GOLD_USD_VOLS");

    // Market data loader
    MockLoader loader;

    // Check commodity volatility construction works
    boost::shared_ptr<CommodityVolCurve> curve;
    BOOST_CHECK_NO_THROW(curve = boost::make_shared<CommodityVolCurve>(asof, curveSpec, loader, curveConfigs));

    // Check volatilities are all equal to the configured volatility regardless of strike and expiry
    Real configuredVolatility = 0.10;
    boost::shared_ptr<BlackVolTermStructure> volatility = curve->volatility();
    BOOST_CHECK_CLOSE(volatility->blackVol(0.25, 1000.0), configuredVolatility, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(0.25, 1200.0), configuredVolatility, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 3 * Months, 1000.0), configuredVolatility, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 3 * Months, 1200.0), configuredVolatility, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(50.0, 1000.0), configuredVolatility, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(50.0, 1200.0), configuredVolatility, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 50 * Years, 1000.0), configuredVolatility, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 50 * Years, 1200.0), configuredVolatility, testTolerance);
}

void CommodityVolCurveTest::testCommodityVolCurveTypeCurve() {

    BOOST_TEST_MESSAGE("Testing commodity vol curve building with time dependent volatilities");

    // Ensure settings are restored after this test
    SavedSettings backup;

    // As of date
    Date asof(5, Feb, 2016);

    // Volatility configuration with time dependent volatilities
    bool extrapolate = true;
    vector<string> quotes {
        "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/1Y/ATMF",
        "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/2Y/ATMF",
        "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/5Y/ATMF"
    };
    boost::shared_ptr<CommodityVolatilityCurveConfig> curveConfig = boost::make_shared<CommodityVolatilityCurveConfig>(
        "GOLD_USD_VOLS", "", "USD", quotes, "A365", "NullCalendar", extrapolate);

    // Curve configurations
    CurveConfigurations curveConfigs;
    curveConfigs.commodityVolatilityCurveConfig("GOLD_USD_VOLS") = curveConfig;

    // Commodity curve spec
    CommodityVolatilityCurveSpec curveSpec("USD", "GOLD_USD_VOLS");

    // Market data loader
    MockLoader loader;

    // Check commodity volatility construction works
    boost::shared_ptr<CommodityVolCurve> curve;
    BOOST_CHECK_NO_THROW(curve = boost::make_shared<CommodityVolCurve>(asof, curveSpec, loader, curveConfigs));

    // Check time depending volatilities are as expected
    boost::shared_ptr<BlackVolTermStructure> volatility = curve->volatility();
    Real configuredVolatility;

    // Check configured pillar points: { (1Y, 0.11), (2Y, 0.10), (5Y, 0.09) }
    // Check also strike independence
    configuredVolatility = 0.11;
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 1 * Years, 1000.0), configuredVolatility, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 1 * Years, 1200.0), configuredVolatility, testTolerance);

    configuredVolatility = 0.10;
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 2 * Years, 1000.0), configuredVolatility, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 2 * Years, 1200.0), configuredVolatility, testTolerance);

    configuredVolatility = 0.09;
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 5 * Years, 1000.0), configuredVolatility, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 5 * Years, 1200.0), configuredVolatility, testTolerance);

    // Check briefly the default linear interpolation and extrapolation
    Time t_s = volatility->dayCounter().yearFraction(asof, asof + 2 * Years); Real v_s = 0.10;
    Time t_e = volatility->dayCounter().yearFraction(asof, asof + 5 * Years); Real v_e = 0.09;
    // at 3 years
    Time t = volatility->dayCounter().yearFraction(asof, asof + 3 * Years);
    Real v = sqrt((v_s * v_s * t_s + (v_e * v_e * t_e - v_s * v_s * t_s) * (t - t_s) / (t_e - t_s)) / t);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 3 * Years, 1000.0), v, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 3 * Years, 1200.0), v, testTolerance);
    // at 6 years, extrapolation is with a flat vol
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 6 * Years, 1000.0), v_e, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 6 * Years, 1200.0), v_e, testTolerance);
}

void CommodityVolCurveTest::testCommodityVolCurveTypeSurface() {

    BOOST_TEST_MESSAGE("Testing commodity vol curve building with time and strike dependent volatilities");

    // Ensure settings are restored after this test
    SavedSettings backup;

    // As of date
    Date asof(5, Feb, 2016);

    // Volatility configuration with time dependent volatilities
    bool extrapolate = true;
    vector<string> expiries { "1Y", "2Y", "5Y" };
    vector<string> strikes { "1150", "1190" };
    boost::shared_ptr<CommodityVolatilityCurveConfig> curveConfig = boost::make_shared<CommodityVolatilityCurveConfig>(
        "GOLD_USD_VOLS", "", "USD", expiries, strikes, "A365", "NullCalendar", extrapolate);

    // Curve configurations
    CurveConfigurations curveConfigs;
    curveConfigs.commodityVolatilityCurveConfig("GOLD_USD_VOLS") = curveConfig;

    // Commodity curve spec
    CommodityVolatilityCurveSpec curveSpec("USD", "GOLD_USD_VOLS");

    // Market data loader
    MockLoader loader;

    // Check commodity volatility construction works
    boost::shared_ptr<CommodityVolCurve> curve;
    BOOST_CHECK_NO_THROW(curve = boost::make_shared<CommodityVolCurve>(asof, curveSpec, loader, curveConfigs));

    // Check time and strike depending volatilities are as expected
    boost::shared_ptr<BlackVolTermStructure> volatility = curve->volatility();

    // Check configured pillar points
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 1 * Years, 1150.0), 0.105, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 1 * Years, 1190.0), 0.115, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 2 * Years, 1150.0), 0.095, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 2 * Years, 1190.0), 0.105, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 5 * Years, 1150.0), 0.085, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 5 * Years, 1190.0), 0.095, testTolerance);
}

test_suite* CommodityVolCurveTest::suite() {

    test_suite* suite = BOOST_TEST_SUITE("CommodityVolCurveTest");

    suite->add(BOOST_TEST_CASE(&CommodityVolCurveTest::testCommodityVolCurveTypeConstant));
    suite->add(BOOST_TEST_CASE(&CommodityVolCurveTest::testCommodityVolCurveTypeCurve));
    suite->add(BOOST_TEST_CASE(&CommodityVolCurveTest::testCommodityVolCurveTypeSurface));

    return suite;
}

}

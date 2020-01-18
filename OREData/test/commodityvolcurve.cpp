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
#include <oret/datapaths.hpp>
#include <oret/toplevelfixture.hpp>

#include <boost/make_shared.hpp>

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/commodityvolcurve.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/marketdata/csvloader.hpp>
#include <qle/termstructures/blackvolsurfacewithatm.hpp>
#include <qle/termstructures/blackvariancesurfacesparse.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>

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
    const vector<Fixing>& loadDividends() const { return dummyDividends_; }
    void add(QuantLib::Date date, const string& name, QuantLib::Real value) {}
    void addFixing(QuantLib::Date date, const string& name, QuantLib::Real value) {}
    void addDividend(Date date, const string& name, Real value) {}

private:
    vector<boost::shared_ptr<MarketDatum>> data_;
    boost::shared_ptr<MarketDatum> dummyDatum_;
    vector<Fixing> dummyFixings_;
    vector<Fixing> dummyDividends_;
};

MockLoader::MockLoader() {
    Date asof(5, Feb, 2016);
    data_ = {
        boost::make_shared<CommodityOptionQuote>(0.11, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/1Y/ATM/AtmFwd",
            MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", boost::make_shared<ExpiryPeriod>(1 * Years), 
            boost::make_shared<AtmStrike>(DeltaVolQuote::AtmFwd)),
        boost::make_shared<CommodityOptionQuote>(0.10, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/2Y/ATM/AtmFwd",
            MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", boost::make_shared<ExpiryPeriod>(2 * Years),
            boost::make_shared<AtmStrike>(DeltaVolQuote::AtmFwd)),
        boost::make_shared<CommodityOptionQuote>(0.09, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/5Y/ATM/AtmFwd",
            MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", boost::make_shared<ExpiryPeriod>(5 * Years),
            boost::make_shared<AtmStrike>(DeltaVolQuote::AtmFwd)),
        boost::make_shared<CommodityOptionQuote>(0.105, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD_USD_VOLS/USD/1Y/1150",
            MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", boost::make_shared<ExpiryPeriod>(1 * Years),
            boost::make_shared<AbsoluteStrike>(1150)),
        boost::make_shared<CommodityOptionQuote>(0.115, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD_USD_VOLS/USD/1Y/1190",
            MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", boost::make_shared<ExpiryPeriod>(1 * Years),
            boost::make_shared<AbsoluteStrike>(1190)),
        boost::make_shared<CommodityOptionQuote>(0.095, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD_USD_VOLS/USD/2Y/1150",
            MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", boost::make_shared<ExpiryPeriod>(2 * Years),
            boost::make_shared<AbsoluteStrike>(1150)),
        boost::make_shared<CommodityOptionQuote>(0.105, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD_USD_VOLS/USD/2Y/1190",
            MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", boost::make_shared<ExpiryPeriod>(2 * Years),
            boost::make_shared<AbsoluteStrike>(1190)),
        boost::make_shared<CommodityOptionQuote>(0.085, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD_USD_VOLS/USD/5Y/1150",
            MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", boost::make_shared<ExpiryPeriod>(5 * Years),
            boost::make_shared<AbsoluteStrike>(1150)),
        boost::make_shared<CommodityOptionQuote>(0.095, asof, "COMMODITY_OPTION/RATE_LNVOL/GOLD_USD_VOLS/USD/5Y/1190",
            MarketDatum::QuoteType::RATE_LNVOL, "GOLD", "USD", boost::make_shared<ExpiryPeriod>(5 * Years),
            boost::make_shared<AbsoluteStrike>(1190))};
}

boost::shared_ptr<TodaysMarket> createTodaysMarket(const Date& asof,
    const string& inputDir, const string& curveConfigFile) {

    Conventions conventions;
    conventions.fromFile(TEST_INPUT_FILE(string(inputDir + "/conventions.xml")));
    
    CurveConfigurations curveConfigs;
    curveConfigs.fromFile(TEST_INPUT_FILE(string(inputDir + "/" + curveConfigFile)));
    
    TodaysMarketParameters todaysMarketParameters;
    todaysMarketParameters.fromFile(TEST_INPUT_FILE(string(inputDir + "/todaysmarket.xml")));

    CSVLoader loader(TEST_INPUT_FILE(string(inputDir + "/market.txt")),
        TEST_INPUT_FILE(string(inputDir + "/fixings.txt")), false);

    return boost::make_shared<TodaysMarket>(asof, todaysMarketParameters, loader, curveConfigs, conventions);
}

// clang-format off
// NYMEX input volatility data that is provided in the input market data file
struct NymexVolatilityData {

    vector<Date> expiries;
    map<Date, vector<Real>> strikes;
    map<Date, vector<Real>> volatilities;
    map<Date, Real> atmVolatilities;

    NymexVolatilityData() {
        expiries = { Date(17, Oct, 2019), Date(16, Dec, 2019), Date(17, Mar, 2020) };
        strikes = {
            { Date(17, Oct, 2019), { 60, 61, 62 } },
            { Date(16, Dec, 2019), { 59, 60, 61 } },
            { Date(17, Mar, 2020), { 57, 58, 59 } }
        };
        volatilities = {
            { Date(17, Oct, 2019), { 0.4516, 0.4558, 0.4598 } },
            { Date(16, Dec, 2019), { 0.4050, 0.4043, 0.4041 } },
            { Date(17, Mar, 2020), { 0.3599, 0.3573, 0.3545 } }
        };
        atmVolatilities = {
            { Date(17, Oct, 2019), 0.4678 },
            { Date(16, Dec, 2019), 0.4353 },
            { Date(17, Mar, 2020), 0.3293 }
        };
    }
};
// clang-format on

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CommodityVolCurveTests)

BOOST_AUTO_TEST_CASE(testCommodityVolCurveTypeConstant) {

    BOOST_TEST_MESSAGE("Testing commodity vol curve building with a single configured volatility");

    // As of date
    Date asof(5, Feb, 2016);

    // Constant volatility config
    auto cvc = boost::make_shared<ConstantVolatilityConfig>("COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/2Y/ATM/AtmFwd");

    // Volatility configuration with a single quote
    boost::shared_ptr<CommodityVolatilityConfig> curveConfig = boost::make_shared<CommodityVolatilityConfig>(
        "GOLD_USD_VOLS", "", "USD", cvc, "A365", "NullCalendar");

    // Curve configurations
    CurveConfigurations curveConfigs;
    curveConfigs.commodityVolatilityConfig("GOLD_USD_VOLS") = curveConfig;

    // Commodity curve spec
    CommodityVolatilityCurveSpec curveSpec("USD", "GOLD_USD_VOLS");

    // Market data loader
    MockLoader loader;

    // Empty Conventions
    Conventions conventions;

    // Check commodity volatility construction works
    boost::shared_ptr<CommodityVolCurve> curve;
    BOOST_CHECK_NO_THROW(curve = boost::make_shared<CommodityVolCurve>(
        asof, curveSpec, loader, curveConfigs, conventions));

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

BOOST_AUTO_TEST_CASE(testCommodityVolCurveTypeCurve) {

    BOOST_TEST_MESSAGE("Testing commodity vol curve building with time dependent volatilities");

    // As of date
    Date asof(5, Feb, 2016);

    // Quotes for the volatility curve
    vector<string> quotes{
        "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/1Y/ATM/AtmFwd",
        "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/2Y/ATM/AtmFwd",
        "COMMODITY_OPTION/RATE_LNVOL/GOLD/USD/5Y/ATM/AtmFwd"
    };

    // Volatility curve config with linear interpolation and flat extrapolation.
    auto vcc = boost::make_shared<VolatilityCurveConfig>(quotes, "Linear", "Flat");

    // Commodity volatility configuration with time dependent volatilities
    boost::shared_ptr<CommodityVolatilityConfig> curveConfig = boost::make_shared<CommodityVolatilityConfig>(
        "GOLD_USD_VOLS", "", "USD", vcc, "A365", "NullCalendar");

    // Curve configurations
    CurveConfigurations curveConfigs;
    curveConfigs.commodityVolatilityConfig("GOLD_USD_VOLS") = curveConfig;

    // Commodity curve spec
    CommodityVolatilityCurveSpec curveSpec("USD", "GOLD_USD_VOLS");

    // Market data loader
    MockLoader loader;

    // Empty Conventions
    Conventions conventions;

    // Check commodity volatility construction works
    boost::shared_ptr<CommodityVolCurve> curve;
    BOOST_CHECK_NO_THROW(curve = boost::make_shared<CommodityVolCurve>(
        asof, curveSpec, loader, curveConfigs, conventions));

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
    Time t_s = volatility->dayCounter().yearFraction(asof, asof + 2 * Years);
    Real v_s = 0.10;
    Time t_e = volatility->dayCounter().yearFraction(asof, asof + 5 * Years);
    Real v_e = 0.09;
    // at 3 years
    Time t = volatility->dayCounter().yearFraction(asof, asof + 3 * Years);
    Real v = sqrt((v_s * v_s * t_s + (v_e * v_e * t_e - v_s * v_s * t_s) * (t - t_s) / (t_e - t_s)) / t);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 3 * Years, 1000.0), v, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 3 * Years, 1200.0), v, testTolerance);
    // at 6 years, extrapolation is with a flat vol
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 6 * Years, 1000.0), v_e, testTolerance);
    BOOST_CHECK_CLOSE(volatility->blackVol(asof + 6 * Years, 1200.0), v_e, testTolerance);
}

BOOST_AUTO_TEST_CASE(testCommodityVolCurveTypeSurface) {

    BOOST_TEST_MESSAGE("Testing commodity vol curve building with time and strike dependent volatilities");

    // As of date
    Date asof(5, Feb, 2016);

    // Volatility configuration with expiry period vs. absolute strike matrix. Bilinear interpolation and flat 
    // extrapolation.
    vector<string> strikes{ "1150", "1190" };
    vector<string> expiries{ "1Y", "2Y", "5Y" };
    auto vssc = boost::make_shared<VolatilityStrikeSurfaceConfig>(
        strikes, expiries, "Linear", "Linear", true, "Flat", "Flat");

    // Commodity volatility configuration
    boost::shared_ptr<CommodityVolatilityConfig> curveConfig = boost::make_shared<CommodityVolatilityConfig>(
        "GOLD_USD_VOLS", "", "USD", vssc, "A365", "NullCalendar");

    // Curve configurations
    CurveConfigurations curveConfigs;
    curveConfigs.commodityVolatilityConfig("GOLD_USD_VOLS") = curveConfig;

    // Commodity curve spec
    CommodityVolatilityCurveSpec curveSpec("USD", "GOLD_USD_VOLS");

    // Market data loader
    MockLoader loader;

    // Empty Conventions
    Conventions conventions;

    // Check commodity volatility construction works
    boost::shared_ptr<CommodityVolCurve> curve;
    BOOST_CHECK_NO_THROW(curve = boost::make_shared<CommodityVolCurve>(
        asof, curveSpec, loader, curveConfigs, conventions));

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

BOOST_AUTO_TEST_CASE(testCommodityVolSurfaceWildcardExpiriesWildcardStrikes) {

    // Testing commodity volatility curve building wildcard expiries and strikes in configuration and more than one 
    // set of commodity volatility quotes in the market data. In particular, the market data in the wildcard_data 
    // folder has commodity volatility data for two surfaces NYMEX:CL and ICE:B. Check here that the commodity 
    // volatility curve building for NYMEX:CL uses only the 9 NYMEX:CL quotes - 3 tenors, each with 3 strikes.
    BOOST_TEST_MESSAGE("Testing commodity volatility curve building wildcard expiries and strikes in configuration");

    auto todaysMarket = createTodaysMarket(Date(16, Sep, 2019), "wildcard_data",
        "curveconfig_surface_wc_expiries_wc_strikes.xml");

    auto vts = todaysMarket->commodityVolatility("NYMEX:CL");

    // Wildcards in configuration so we know that a BlackVarianceSurfaceSparse has been created and fed to a 
    // BlackVolatilityWithATM surface in TodaysMarket
    auto tmSurface = boost::dynamic_pointer_cast<BlackVolatilityWithATM>(*vts);
    BOOST_REQUIRE_MESSAGE(tmSurface, "Expected the commodity vol structure in TodaysMarket" <<
        " to be of type BlackVolatilityWithATM");
    auto surface = boost::dynamic_pointer_cast<BlackVarianceSurfaceSparse>(tmSurface->surface());
    BOOST_REQUIRE_MESSAGE(tmSurface, "Expected the commodity vol structure in TodaysMarket to contain" <<
        " a surface of type BlackVarianceSurfaceSparse");

    // The expected NYMEX CL volatility data
    NymexVolatilityData expData;

    // Check what is loaded against expected data as provided in market data file for NYMEX:CL.
    // Note: the BlackVarianceSurfaceSparse adds a dummy expiry slice at time zero
    BOOST_REQUIRE_EQUAL(surface->expiries().size() - 1, expData.expiries.size());
    for (Size i = 0; i < expData.strikes.size(); i++) {
        BOOST_CHECK_EQUAL(surface->expiries()[i + 1], expData.expiries[i]);
    }
    
    BOOST_REQUIRE_EQUAL(surface->strikes().size() - 1, expData.strikes.size());
    for (Size i = 0; i < expData.strikes.size(); i++) {

        // Check the strikes against the input
        Date e = expData.expiries[i];
        BOOST_REQUIRE_EQUAL(surface->strikes()[i + 1].size(), expData.strikes[e].size());
        for (Size j = 0; j < surface->strikes()[i + 1].size(); j++) {
            BOOST_CHECK_CLOSE(expData.strikes[e][j], surface->strikes()[i + 1][j], 1e-12);
        }
        
        // Check the volatilities against the input
        BOOST_REQUIRE_EQUAL(surface->values()[i + 1].size(), expData.volatilities[e].size());
        for (Size j = 0; j < surface->values()[i + 1].size(); j++) {
            BOOST_CHECK_CLOSE(expData.volatilities[e][j], surface->blackVol(e, surface->strikes()[i + 1][j]), 1e-12);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCommodityVolSurfaceWildcardExpiriesExplicitStrikes) {

    BOOST_TEST_MESSAGE("Testing commodity volatility curve building wildcard expiries and explicit strikes in configuration");

    auto todaysMarket = createTodaysMarket(Date(16, Sep, 2019), "wildcard_data",
        "curveconfig_surface_wc_expiries_explicit_strikes.xml");

    auto vts = todaysMarket->commodityVolatility("NYMEX:CL");

    // Wildcards in configuration so we know that a BlackVarianceSurfaceSparse has been created and fed to a 
    // BlackVolatilityWithATM surface in TodaysMarket
    auto tmSurface = boost::dynamic_pointer_cast<BlackVolatilityWithATM>(*vts);
    BOOST_REQUIRE_MESSAGE(tmSurface, "Expected the commodity vol structure in TodaysMarket" <<
        " to be of type BlackVolatilityWithATM");
    auto surface = boost::dynamic_pointer_cast<BlackVarianceSurfaceSparse>(tmSurface->surface());
    BOOST_REQUIRE_MESSAGE(tmSurface, "Expected the commodity vol structure in TodaysMarket to contain" <<
        " a surface of type BlackVarianceSurfaceSparse");

    // The expected NYMEX CL volatility data
    NymexVolatilityData expData;

    // Check what is loaded against expected data as provided in market data file for NYMEX:CL. The explicit strikes
    // that we have chosen lead have only two corresponding expiries i.e. 2019-10-17 and 2019-12-16
    // Note: the BlackVarianceSurfaceSparse adds a dummy expiry slice at time zero
    vector<Date> expExpiries{ Date(17, Oct, 2019), Date(16, Dec, 2019) };
    BOOST_REQUIRE_EQUAL(surface->expiries().size() - 1, expExpiries.size());
    for (Size i = 0; i < expExpiries.size(); i++) {
        BOOST_CHECK_EQUAL(surface->expiries()[i + 1], expExpiries[i]);
    }

    // The explicit strikes in the config are 60 and 61
    vector<Real> expStrikes{ 60, 61 };
    BOOST_REQUIRE_EQUAL(surface->strikes().size() - 1, expExpiries.size());
    for (Size i = 0; i < expExpiries.size(); i++) {

        // Check the strikes against the expected explicit strikes
        BOOST_REQUIRE_EQUAL(surface->strikes()[i + 1].size(), expStrikes.size());
        for (Size j = 0; j < surface->strikes()[i + 1].size(); j++) {
            BOOST_CHECK_CLOSE(expStrikes[j], surface->strikes()[i + 1][j], 1e-12);
        }

        // Check the volatilities against the input
        Date e = expExpiries[i];
        BOOST_REQUIRE_EQUAL(surface->values()[i + 1].size(), expStrikes.size());
        for (Size j = 0; j < surface->values()[i + 1].size(); j++) {
            // Find the index of the explicit strike in the input data
            Real expStrike = expStrikes[j];
            auto it = find_if(expData.strikes[e].begin(), expData.strikes[e].end(),
                [expStrike](Real s) { return close(expStrike, s); });
            BOOST_REQUIRE_MESSAGE(it != expData.strikes[e].end(), "Strike not found in input strikes");
            auto idx = distance(expData.strikes[e].begin(), it);

            BOOST_CHECK_CLOSE(expData.volatilities[e][idx], surface->blackVol(e, surface->strikes()[i + 1][j]), 1e-12);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCommodityVolSurfaceExplicitExpiriesWildcardStrikes) {

    BOOST_TEST_MESSAGE("Testing commodity volatility curve building explicit expiries and wildcard strikes in configuration");

    auto todaysMarket = createTodaysMarket(Date(16, Sep, 2019), "wildcard_data",
        "curveconfig_surface_explicit_expiries_wc_strikes.xml");

    auto vts = todaysMarket->commodityVolatility("NYMEX:CL");

    // Wildcards in configuration so we know that a BlackVarianceSurfaceSparse has been created and fed to a 
    // BlackVolatilityWithATM surface in TodaysMarket
    auto tmSurface = boost::dynamic_pointer_cast<BlackVolatilityWithATM>(*vts);
    BOOST_REQUIRE_MESSAGE(tmSurface, "Expected the commodity vol structure in TodaysMarket" <<
        " to be of type BlackVolatilityWithATM");
    auto surface = boost::dynamic_pointer_cast<BlackVarianceSurfaceSparse>(tmSurface->surface());
    BOOST_REQUIRE_MESSAGE(tmSurface, "Expected the commodity vol structure in TodaysMarket to contain" <<
        " a surface of type BlackVarianceSurfaceSparse");

    // The expected NYMEX CL volatility data
    NymexVolatilityData expData;

    // Check what is loaded against expected data as provided in market data file for NYMEX:CL.
    // We have chosen the explicit expiries 2019-10-17 and 2019-12-16
    // Note: the BlackVarianceSurfaceSparse adds a dummy expiry slice at time zero
    vector<Date> expExpiries{ Date(17, Oct, 2019), Date(16, Dec, 2019) };
    BOOST_REQUIRE_EQUAL(surface->expiries().size() - 1, expExpiries.size());
    for (Size i = 0; i < expExpiries.size(); i++) {
        BOOST_CHECK_EQUAL(surface->expiries()[i + 1], expExpiries[i]);
    }

    BOOST_REQUIRE_EQUAL(surface->strikes().size() - 1, expExpiries.size());
    for (Size i = 0; i < expExpiries.size(); i++) {

        // Check the strikes against the input
        Date e = expExpiries[i];
        BOOST_REQUIRE_EQUAL(surface->strikes()[i + 1].size(), expData.strikes[e].size());
        for (Size j = 0; j < surface->strikes()[i + 1].size(); j++) {
            BOOST_CHECK_CLOSE(expData.strikes[e][j], surface->strikes()[i + 1][j], 1e-12);
        }

        // Check the volatilities against the input
        BOOST_REQUIRE_EQUAL(surface->values()[i + 1].size(), expData.volatilities[e].size());
        for (Size j = 0; j < surface->values()[i + 1].size(); j++) {
            BOOST_CHECK_CLOSE(expData.volatilities[e][j], surface->blackVol(e, surface->strikes()[i + 1][j]), 1e-12);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCommodityVolSurfaceExplicitExpiriesExplicitStrikes) {

    BOOST_TEST_MESSAGE("Testing commodity volatility curve building explicit expiries and explicit strikes in configuration");

    auto todaysMarket = createTodaysMarket(Date(16, Sep, 2019), "wildcard_data",
        "curveconfig_surface_explicit_expiries_explicit_strikes.xml");

    auto vts = todaysMarket->commodityVolatility("NYMEX:CL");

    // The expected NYMEX CL volatility data
    NymexVolatilityData expData;

    // We have provided two explicit expiries, 2019-10-17 and 2019-12-16, and two explicit strikes, 60 and 61.
    // We check the volatility term structure at these 4 points against the input data
    vector<Date> expExpiries{ Date(17, Oct, 2019), Date(16, Dec, 2019) };
    vector<Real> expStrikes{ 60, 61 };
    for (const Date& e : expExpiries) {
        for (Real s : expStrikes) {
            // Find the index of the explicit strike in the input data
            auto it = find_if(expData.strikes[e].begin(), expData.strikes[e].end(),
                [s](Real strike) { return close(strike, s); });
            BOOST_REQUIRE_MESSAGE(it != expData.strikes[e].end(), "Strike not found in input strikes");
            auto idx = distance(expData.strikes[e].begin(), it);

            Real inputVol = expData.volatilities[e][idx];
            BOOST_CHECK_CLOSE(inputVol, vts->blackVol(e, s), 1e-12);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

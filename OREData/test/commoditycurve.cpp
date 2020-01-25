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

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <oret/datapaths.hpp>
#include <oret/toplevelfixture.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/commoditycurve.hpp>
#include <ored/marketdata/csvloader.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/utilities/to_string.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;

namespace bdata = boost::unit_test::data;

namespace {

// List of curve configuration file names for data test case below
vector<string> curveConfigFiles = {
    "curveconfig_linear.xml",
    "curveconfig_linear_flat.xml",
    "curveconfig_loglinear.xml",
    "curveconfig_loglinear_flat.xml",
    "curveconfig_cubic.xml",
    "curveconfig_cubic_flat.xml"
};

// The expected commodity curve
map<Date, Real> expectedCurve = {
    { Date(29, Jul, 2019), 1417.8998900 },
    { Date(30, Jul, 2019), 1417.9999450 },
    { Date(31, Jul, 2019), 1418.1000000 },
    { Date( 1, Aug, 2019), 1418.2000550 },
    { Date(30, Aug, 2019), 1421.1016535 },
    { Date(30, Sep, 2019), 1424.1312750 }
};

// Pillars for interpolated curve tests (purposely leave out elements below spot to test interpolation there)
map<Date, Real> expectedInterpCurvePillars = {
    { Date(31, Jul, 2019), 1418.1000000 },
    { Date( 1, Aug, 2019), 1418.2000550 },
    { Date(30, Aug, 2019), 1421.1016535 },
    { Date(30, Sep, 2019), 1424.1312750 }
};

// Expected results for extrapolation below spot, interpolation and extrapolation beyond max date for the 
// various interpolation methods
vector<Date> offPillarDates = { Date(29, Jul, 2019), Date(15, Sep, 2019), Date(1, Nov, 2019) };

map<string, vector<Real>> expectedInterpCurveOffPillars = {
    { "curveconfig_linear.xml", { 1417.89989, 1422.6653291129, 1427.2586262258 } },
    { "curveconfig_linear_flat.xml", { 1418.1, 1422.6653291129, 1424.131275 } },
    { "curveconfig_loglinear.xml", { 1417.89991117635, 1422.66452345277, 1427.26540106042 } },
    { "curveconfig_loglinear_flat.xml", { 1418.1, 1422.66452345277, 1424.131275 } },
    { "curveconfig_cubic.xml", { 1417.89988981896, 1422.67192914531, 1427.25983144911 } },
    { "curveconfig_cubic_flat.xml", { 1418.1, 1422.67192914531, 1424.131275 } }
};


boost::shared_ptr<CommodityCurve> createCurve(const string& inputDir,
    const Date& asof,
    const CommodityCurveSpec& curveSpec,
    const string& curveConfigFile = "curveconfig.xml",
    const string& fixingsFile = "fixings.txt") {
    
    Conventions conventions;
    string filename = inputDir + "/conventions.xml";
    conventions.fromFile(TEST_INPUT_FILE(filename));
    CurveConfigurations curveConfigs;
    filename = inputDir + "/" + curveConfigFile;
    curveConfigs.fromFile(TEST_INPUT_FILE(filename));
    filename = inputDir + "/market.txt";
    CSVLoader loader(TEST_INPUT_FILE(filename), TEST_INPUT_FILE(fixingsFile), false);

    // Check commodity curve construction works
    boost::shared_ptr<CommodityCurve> curve;
    BOOST_REQUIRE_NO_THROW(curve = boost::make_shared<CommodityCurve>(
        asof, curveSpec, loader, curveConfigs, conventions));

    return curve;
}

void checkCurve(const boost::shared_ptr<PriceTermStructure>& priceCurve, const map<Date, Real>& expectedValues) {

    for (const auto& kv : expectedValues) {
        Real price = priceCurve->price(kv.first);
        BOOST_CHECK_CLOSE(price, kv.second, 1e-12);
    }
}

}

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CommodityCurveTests)

BOOST_AUTO_TEST_CASE(testCommodityCurveTenorBasedOnTnPoints) {
    
    BOOST_TEST_MESSAGE("Testing commodity curve building with tenor based points quotes including ON and TN");
    
    Date asof(29, Jul, 2019);
    CommodityCurveSpec curveSpec("USD", "PM:XAUUSD");
    
    boost::shared_ptr<CommodityCurve> curve = createCurve("tenor_based_on_tn_points", asof, curveSpec);
    checkCurve(curve->commodityPriceCurve(), expectedCurve);
}

BOOST_AUTO_TEST_CASE(testCommodityCurveFixedDatePoints) {

    BOOST_TEST_MESSAGE("Testing commodity curve building with fixed date quotes");

    Date asof(29, Jul, 2019);
    CommodityCurveSpec curveSpec("USD", "PM:XAUUSD");

    boost::shared_ptr<CommodityCurve> curve = createCurve("fixed_date_points", asof, curveSpec);
    checkCurve(curve->commodityPriceCurve(), expectedCurve);
}

// Testing various interpolation methods
BOOST_DATA_TEST_CASE(testCommodityInterpolations, bdata::make(curveConfigFiles), curveConfigFile) {

    BOOST_TEST_MESSAGE("Testing with configuration file: " << curveConfigFile);

    Date asof(29, Jul, 2019);
    CommodityCurveSpec curveSpec("USD", "PM:XAUUSD");

    boost::shared_ptr<CommodityCurve> curve = createCurve("different_interpolations",
        asof, curveSpec, curveConfigFile);
    checkCurve(curve->commodityPriceCurve(), expectedInterpCurvePillars);

    BOOST_REQUIRE(expectedInterpCurveOffPillars.count(curveConfigFile) == 1);
    for (Size i = 0; i < offPillarDates.size(); i++) {
        Real price = curve->commodityPriceCurve()->price(offPillarDates[i]);
        Real expPrice = expectedInterpCurveOffPillars.at(curveConfigFile)[i];
        BOOST_CHECK_CLOSE(price, expPrice, 1e-12);
    }
}

BOOST_AUTO_TEST_CASE(testCommodityBasisCurve) {

    BOOST_TEST_MESSAGE("Testing commodity basis curve building...");

    Date asof(30, Sep, 2019);
    CommodityCurveSpec curveSpec("USD", "NYMEX:FF");
    string dir = "basis/wti_midland_cm";
    string fixingsFile = dir + "/fixings_" + to_string(io::iso_date(asof)) + ".txt";

    boost::shared_ptr<CommodityCurve> curve;
    BOOST_REQUIRE_NO_THROW(curve = createCurve(dir, asof, curveSpec, "curveconfig.xml", fixingsFile));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

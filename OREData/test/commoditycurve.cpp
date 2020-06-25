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
// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on
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
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/to_string.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;

namespace bdata = boost::unit_test::data;

namespace {

// List of curve configuration file names for data test case below
vector<string> curveConfigFiles = {"curveconfig_linear.xml",    "curveconfig_linear_flat.xml",
                                   "curveconfig_loglinear.xml", "curveconfig_loglinear_flat.xml",
                                   "curveconfig_cubic.xml",     "curveconfig_cubic_flat.xml"};

// The expected commodity curve
map<Date, Real> expectedCurve = {{Date(29, Jul, 2019), 1417.8998900}, {Date(30, Jul, 2019), 1417.9999450},
                                 {Date(31, Jul, 2019), 1418.1000000}, {Date(1, Aug, 2019), 1418.2000550},
                                 {Date(30, Aug, 2019), 1421.1016535}, {Date(30, Sep, 2019), 1424.1312750}};

// Pillars for interpolated curve tests (purposely leave out elements below spot to test interpolation there)
map<Date, Real> expectedInterpCurvePillars = {{Date(31, Jul, 2019), 1418.1000000},
                                              {Date(1, Aug, 2019), 1418.2000550},
                                              {Date(30, Aug, 2019), 1421.1016535},
                                              {Date(30, Sep, 2019), 1424.1312750}};

// Expected results for extrapolation below spot, interpolation and extrapolation beyond max date for the
// various interpolation methods
vector<Date> offPillarDates = {Date(29, Jul, 2019), Date(15, Sep, 2019), Date(1, Nov, 2019)};

map<string, vector<Real>> expectedInterpCurveOffPillars = {
    {"curveconfig_linear.xml", {1417.89989, 1422.6653291129, 1427.2586262258}},
    {"curveconfig_linear_flat.xml", {1418.1, 1422.6653291129, 1424.131275}},
    {"curveconfig_loglinear.xml", {1417.89991117635, 1422.66452345277, 1427.26540106042}},
    {"curveconfig_loglinear_flat.xml", {1418.1, 1422.66452345277, 1424.131275}},
    {"curveconfig_cubic.xml", {1417.89988981896, 1422.67192914531, 1427.25983144911}},
    {"curveconfig_cubic_flat.xml", {1418.1, 1422.67192914531, 1424.131275}}};

boost::shared_ptr<CommodityCurve> createCurve(const string& inputDir,
                                              const string& curveConfigFile = "curveconfig.xml") {

    // As of date
    Date asof(29, Jul, 2019);

    Conventions conventions;
    string filename = inputDir + "/conventions.xml";
    conventions.fromFile(TEST_INPUT_FILE(filename));
    CurveConfigurations curveConfigs;
    filename = inputDir + "/" + curveConfigFile;
    curveConfigs.fromFile(TEST_INPUT_FILE(filename));
    filename = inputDir + "/market.txt";
    CSVLoader loader(TEST_INPUT_FILE(filename), TEST_INPUT_FILE("fixings.txt"), false);

    // Commodity curve spec
    CommodityCurveSpec curveSpec("USD", "PM:XAUUSD");

    // Check commodity curve construction works
    boost::shared_ptr<CommodityCurve> curve;
    BOOST_REQUIRE_NO_THROW(curve =
                               boost::make_shared<CommodityCurve>(asof, curveSpec, loader, curveConfigs, conventions));

    return curve;
}

boost::shared_ptr<TodaysMarket> createTodaysMarket(const Date& asof, const string& inputDir) {

    Conventions conventions;
    conventions.fromFile(TEST_INPUT_FILE(string(inputDir + "/conventions.xml")));

    CurveConfigurations curveConfigs;
    curveConfigs.fromFile(TEST_INPUT_FILE(string(inputDir + "/curveconfig.xml")));

    TodaysMarketParameters todaysMarketParameters;
    todaysMarketParameters.fromFile(TEST_INPUT_FILE(string(inputDir + "/todaysmarket.xml")));

    string fixingsFile = inputDir + "/fixings_" + to_string(io::iso_date(asof)) + ".txt";
    CSVLoader loader(TEST_INPUT_FILE(string(inputDir + "/market.txt")), TEST_INPUT_FILE(fixingsFile), false);

    return boost::make_shared<TodaysMarket>(asof, todaysMarketParameters, loader, curveConfigs, conventions);
}

void checkCurve(const boost::shared_ptr<PriceTermStructure>& priceCurve, const map<Date, Real>& expectedValues) {

    for (const auto& kv : expectedValues) {
        Real price = priceCurve->price(kv.first);
        BOOST_CHECK_CLOSE(price, kv.second, 1e-12);
    }
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CommodityCurveTests)

BOOST_AUTO_TEST_CASE(testCommodityCurveTenorBasedOnTnPoints) {

    BOOST_TEST_MESSAGE("Testing commodity curve building with tenor based points quotes including ON and TN");

    boost::shared_ptr<CommodityCurve> curve = createCurve("tenor_based_on_tn_points");
    checkCurve(curve->commodityPriceCurve(), expectedCurve);
}

BOOST_AUTO_TEST_CASE(testCommodityCurveFixedDatePoints) {

    BOOST_TEST_MESSAGE("Testing commodity curve building with fixed date quotes");

    boost::shared_ptr<CommodityCurve> curve = createCurve("fixed_date_points");
    checkCurve(curve->commodityPriceCurve(), expectedCurve);
}

// Testing various interpolation methods
BOOST_DATA_TEST_CASE(testCommodityInterpolations, bdata::make(curveConfigFiles), curveConfigFile) {

    BOOST_TEST_MESSAGE("Testing with configuration file: " << curveConfigFile);

    boost::shared_ptr<CommodityCurve> curve = createCurve("different_interpolations", curveConfigFile);
    checkCurve(curve->commodityPriceCurve(), expectedInterpCurvePillars);

    BOOST_REQUIRE(expectedInterpCurveOffPillars.count(curveConfigFile) == 1);
    for (Size i = 0; i < offPillarDates.size(); i++) {
        Real price = curve->commodityPriceCurve()->price(offPillarDates[i]);
        Real expPrice = expectedInterpCurveOffPillars.at(curveConfigFile)[i];
        BOOST_CHECK_CLOSE(price, expPrice, 1e-12);
    }
}

// Basis test cases for data test below.
struct BasisTestCase {
    Date asof;
    string name;
    string curveName;
};

// List of basis test case directories for the data test case below.
vector<BasisTestCase> basisTestCases = {{Date(30, Sep, 2019), "wti_midland_cm", "NYMEX:FF"},
                                        {Date(30, Sep, 2019), "wti_midland_tm", "NYMEX:WTT"},
                                        {Date(30, Sep, 2019), "wti_midland_cm_base_ave", "NYMEX:FF"},
                                        {Date(30, Sep, 2019), "houston_ship_channel", "ICE:HXS"},
                                        {Date(23, Jan, 2020), "wti_midland_cm", "NYMEX:FF"},
                                        {Date(23, Jan, 2020), "wti_midland_tm", "NYMEX:WTT"},
                                        {Date(23, Jan, 2020), "wti_midland_cm_base_ave", "NYMEX:FF"},
                                        {Date(23, Jan, 2020), "houston_ship_channel", "ICE:HXS"}};

// Needed for BOOST_DATA_TEST_CASE below as it writes out the case.
ostream& operator<<(ostream& os, BasisTestCase testCase) {
    return os << "[" << io::iso_date(testCase.asof) << "," << testCase.name << "," << testCase.curveName << "]";
}

BOOST_DATA_TEST_CASE(testCommodityBasisCurve, bdata::make(basisTestCases), basisTestCase) {

    BOOST_TEST_MESSAGE("Testing commodity basis curve building " << basisTestCase << "...");

    Settings::instance().evaluationDate() = basisTestCase.asof;
    string dir = "basis/" + basisTestCase.name;

    boost::shared_ptr<TodaysMarket> tm;
    BOOST_REQUIRE_NO_THROW(tm = createTodaysMarket(basisTestCase.asof, dir));

    auto pts = tm->commodityPriceCurve(basisTestCase.curveName);

    for (const Date& d : pts->pillarDates()) {
        BOOST_TEST_MESSAGE(io::iso_date(d) << "," << fixed << setprecision(12) << pts->price(d));
    }

    // Tolerance for float comparison
    Real tol = 1e-12;

    // Read in the expected pillar results for the given date.
    vector<Date> expPillarDates;
    string filename = dir + "/expected_" + to_string(io::iso_date(basisTestCase.asof)) + ".csv";
    CSVFileReader reader(TEST_INPUT_FILE(filename), true, ",");
    BOOST_REQUIRE_EQUAL(reader.numberOfColumns(), 2);

    while (reader.next()) {
        // Get the expected expiry pillar date and price.
        Date expiry = parseDate(reader.get(0));
        Real price = parseReal(reader.get(1));
        expPillarDates.push_back(expiry);

        // Check the surface on the grid point.
        Real calcPrice = pts->price(expiry);
        BOOST_TEST_MESSAGE(io::iso_date(expiry) << "," << fixed << setprecision(12) << calcPrice);
        BOOST_CHECK_SMALL(price - calcPrice, tol);
    }

    vector<Date> calcPillarDates = pts->pillarDates();
    BOOST_CHECK_EQUAL_COLLECTIONS(expPillarDates.begin(), expPillarDates.end(), calcPillarDates.begin(),
                                  calcPillarDates.end());

    // Set up has flat extrapolation. Check it here.
    Real lastPrice = pts->price(calcPillarDates.back());
    Date extrapDate = calcPillarDates.back() + 1 * Years;
    Real extrapPrice = pts->price(extrapDate);
    BOOST_CHECK_SMALL(lastPrice - extrapPrice, tol);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

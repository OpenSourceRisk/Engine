/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on
#include <oret/datapaths.hpp>
#include <oret/toplevelfixture.hpp>

#include <boost/make_shared.hpp>

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/csvloader.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/parsers.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;

namespace bdata = boost::unit_test::data;

namespace {

QuantLib::ext::shared_ptr<TodaysMarket> createTodaysMarket(const Date& asof, const string& inputDir) {

    auto conventions = QuantLib::ext::make_shared<Conventions>();
    // conventions->fromFile(TEST_INPUT_FILE(string(inputDir + "/conventions.xml")));
    InstrumentConventions::instance().setConventions(conventions);
    
    auto curveConfigs = QuantLib::ext::make_shared<CurveConfigurations>();
    curveConfigs->fromFile(TEST_INPUT_FILE(string(inputDir + "/curveconfig.xml")));

    auto todaysMarketParameters = QuantLib::ext::make_shared<TodaysMarketParameters>();
    todaysMarketParameters->fromFile(TEST_INPUT_FILE(string(inputDir + "/todaysmarket.xml")));

    auto loader = QuantLib::ext::make_shared<CSVLoader>(TEST_INPUT_FILE(string(inputDir + "/market.txt")),
                                                TEST_INPUT_FILE(string(inputDir + "/fixings.txt")), false);

    return QuantLib::ext::make_shared<TodaysMarket>(asof, todaysMarketParameters, loader, curveConfigs);
}

}

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BaseCorrelationCurveTests)

// Sub-directories containing input data to test various base correlation curve and market data set-ups
vector<string> setups{
    "exp_terms_exp_dps_curve",
    "exp_terms_exp_dps_surface",
    "exp_terms_wc_dps_curve",
    "exp_terms_wc_dps_surface",
    "wc_terms_exp_dps_curve",
    "wc_terms_exp_dps_surface",
    "wc_terms_wc_dps_curve",
    "wc_terms_wc_dps_surface"
};

BOOST_DATA_TEST_CASE(testBaseCorrelationStructureBuilding, bdata::make(setups), setup) {

    BOOST_TEST_MESSAGE("Testing base correlation structure building using setup in " << setup);

    Date asof(19, Oct, 2020);
    Settings::instance().evaluationDate() = asof;

    auto todaysMarket = createTodaysMarket(asof, setup);

    // Get the built base correlation structure.
    auto bc = todaysMarket->baseCorrelation("BASE_CORR_TEST");

    // These are the values used in the test configurations.
    Calendar calendar = parseCalendar("US settlement");
    BusinessDayConvention bdc = Following;

    // Tolerance for comparison.
    Real tol = 1e-12;

    // Read in the expected results.
    string filename = setup + "/expected.csv";
    CSVFileReader reader(TEST_INPUT_FILE(filename), true, ",");
    BOOST_REQUIRE_EQUAL(reader.numberOfColumns(), 3);

    BOOST_TEST_MESSAGE("term,detachment,expected_bc,calculated_bc,difference");
    while (reader.next()) {

        // Get the term, detachment point and expected base correlation
        Period term = parsePeriod(reader.get(0));
        Real dp = parseReal(reader.get(1));
        Real expBc = parseReal(reader.get(2));

        // Check
        Date d = calendar.advance(asof, term, bdc);
        Real calcBc = bc->correlation(d, dp);
        Real difference = expBc - calcBc;
        BOOST_TEST_MESSAGE(term << "," << fixed << setprecision(12) << dp << "," <<
            expBc << "," << calcBc << "," << difference);
        BOOST_CHECK_SMALL(difference, tol);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

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

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;

namespace {

// The expected commodity curve inclusive of ON and TN
map<Date, Real> expectedCurveOnTn = {
    { Date(29, Jul, 2019), 1417.8998900 },
    { Date(30, Jul, 2019), 1417.9999450 },
    { Date(31, Jul, 2019), 1418.1000000 },
    { Date( 1, Aug, 2019), 1418.2000550 },
    { Date(30, Aug, 2019), 1421.1016535 },
    { Date(30, Sep, 2019), 1424.1312750 }
};

// The expected commodity curve without ON and TN
map<Date, Real> expectedCurveNoOnTn = {
    { Date(29, Jul, 2019), 1418.1000000 },
    { Date(30, Jul, 2019), 1418.1000000 },
    { Date(31, Jul, 2019), 1418.1000000 },
    { Date( 1, Aug, 2019), 1418.2000550 },
    { Date(30, Aug, 2019), 1421.1016535 },
    { Date(30, Sep, 2019), 1424.1312750 }
};

boost::shared_ptr<CommodityCurve> createCurve(const string& inputDir) {
    
    // As of date
    Date asof(29, Jul, 2019);

    Conventions conventions;
    string filename = inputDir + "/conventions.xml";
    conventions.fromFile(TEST_INPUT_FILE(filename));
    CurveConfigurations curveConfigs;
    filename = inputDir + "/curveconfig.xml";
    curveConfigs.fromFile(TEST_INPUT_FILE(filename));
    filename = inputDir + "/market.txt";
    CSVLoader loader(TEST_INPUT_FILE(filename), TEST_INPUT_FILE("fixings.txt"), false);

    // Commodity curve spec
    CommodityCurveSpec curveSpec("USD", "PM:XAUUSD");

    // Check commodity curve construction works
    boost::shared_ptr<CommodityCurve> curve;
    BOOST_REQUIRE_NO_THROW(curve = boost::make_shared<CommodityCurve>(
        asof, curveSpec, loader, curveConfigs, conventions));

    return curve;
}

void checkCurve(const boost::shared_ptr<PriceTermStructure>& priceCurve, bool includeOnTn) {

    map<Date, Real> expected = includeOnTn ? expectedCurveOnTn : expectedCurveNoOnTn;

    for (const auto& kv : expected) {
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
    checkCurve(curve->commodityPriceCurve(), true);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

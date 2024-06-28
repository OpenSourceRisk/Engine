/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ored/utilities/conventionsbasedfutureexpiry.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/parsers.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace ore::data;

namespace bdata = boost::unit_test::data;

// List of commodity names for data test case below
vector<string> commodityNames = {
    "ice_brent",
    "ice_brent_old",
    "nymex_cl",
    "nymex_ng",
    "ice_cig_basis",
    "myr_palm_oil",
    "ice_wti_midland_basis",
    "ice_pmi",
    "ice_erh",
    "ice_eiw",
    "ice_hen_basis",
    "ice_his_basis",
    "cme_myr_palm_oil"
};

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ConventionsBasedFutureExpiryTests)

BOOST_DATA_TEST_CASE(testExpiryDates, bdata::make(commodityNames), commodityName) {

    BOOST_TEST_MESSAGE("Testing expiry dates for commodity: " << commodityName);

    // Read in the relevant conventions file
    Conventions conventions;
    string filename = commodityName + "_conventions.xml";
    conventions.fromFile(TEST_INPUT_FILE(filename));

    // Create the conventions based expiry calculator
    BOOST_TEST_REQUIRE(conventions.has(commodityName));
    auto convention = QuantLib::ext::dynamic_pointer_cast<CommodityFutureConvention>(conventions.get(commodityName));
    BOOST_TEST_REQUIRE(convention);
    ConventionsBasedFutureExpiry cbfe(*convention);

    // Read in the contract months and expected expiry dates
    filename = commodityName + "_expiries.csv";
    CSVFileReader reader(TEST_INPUT_FILE(filename), true, ",");
    BOOST_REQUIRE_EQUAL(reader.numberOfColumns(), 3);

    while (reader.next()) {

        // Get the contract date and the expected expiry date from the file
        Date contractDate = parseDate(reader.get(0));
        Date expExpiryDate = parseDate(reader.get(1));

        // Calculate the expiry date using the future expiry calculator
        Date expiryDate = cbfe.expiryDate(contractDate, 0);

        // Check that the calculated expiry equals the expected expiry date
        BOOST_CHECK_EQUAL(expExpiryDate, expiryDate);

        // If there is an expected option expiry date, test that also
        string strExpOptionExpiry = reader.get(2);
        if (!strExpOptionExpiry.empty()) {

            // Expected option expiry date
            Date expOptionExpiry = parseDate(strExpOptionExpiry);

            // Calculate the option expiry date using the future expiry calculator
            Date optionExpiry = cbfe.expiryDate(contractDate, 0, true);

            BOOST_CHECK_EQUAL(expOptionExpiry, optionExpiry);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

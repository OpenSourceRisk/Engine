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

#include <test/sensitivityaggregator.hpp>

#include <orea/engine/sensitivityaggregator.hpp>
#include <orea/engine/sensitivityinmemorystream.hpp>
#include <ql/math/comparison.hpp>

using namespace boost::unit_test_framework;

using ore::analytics::RiskFactorKey;
using ore::analytics::SensitivityRecord;
using ore::analytics::SensitivityInMemoryStream;
using ore::analytics::SensitivityAggregator;
using std::set;
using std::map;

namespace testsuite {

using RFType = RiskFactorKey::KeyType;

// Sensitivity records for aggregation
// clang-format off
static const set<SensitivityRecord> records = {
    { "trade_001", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 3), "6M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -103053.46, 74.06, 0.00},
    { "trade_001", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 4), "1Y", 0.0001, RiskFactorKey(), "", 0.0, "USD", -103053.46, 354.79, -0.03 },
    { "trade_001", false, RiskFactorKey(RFType::DiscountCurve, "USD", 3), "6M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -103053.46, -72.54, 0.00 },
    { "trade_001", false, RiskFactorKey(RFType::DiscountCurve, "USD", 4), "1Y", 0.0001, RiskFactorKey(), "", 0.0, "USD", -103053.46, -347.52, 0.02 },
    { "trade_001", false, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, RiskFactorKey(), "", 0.0, "USD", -103053.46, -50331.89, 0.00 },
    { "trade_003", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 1), "1M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -156337.99, 38.13, 0.00 },
    { "trade_003", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 2), "3M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -156337.99, 114.53, 0.00 },
    { "trade_003", false, RiskFactorKey(RFType::DiscountCurve, "USD", 1), "1M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -156337.99, -37.48, 0.00 },
    { "trade_003", false, RiskFactorKey(RFType::DiscountCurve, "USD", 2), "3M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -156337.99, -112.57, 0.00 },
    { "trade_003", false, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, RiskFactorKey(), "", 0.0, "USD", -156337.99, -91345.92, 0.00 },
    { "trade_004", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 3), "6M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -110809.22, 27.11, 0.00 },
    { "trade_004", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 4), "1Y", 0.0001, RiskFactorKey(), "", 0.0, "USD", -110809.22, 940.54, -0.09 },
    { "trade_004", false, RiskFactorKey(RFType::DiscountCurve, "USD", 3), "6M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -110809.22, -26.81, 0.00 },
    { "trade_004", false, RiskFactorKey(RFType::DiscountCurve, "USD", 4), "1Y", 0.0001, RiskFactorKey(), "", 0.0, "USD", -110809.22, -930.06, 0.09 },
    { "trade_004", false, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, RiskFactorKey(), "", 0.0, "USD", -110809.22, -99495.14, 0.00 }
};
// clang-format on

// Expected result of aggregation over all elements above
// clang-format off
set<SensitivityRecord> expAggregationAll = {
    { "", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 1), "1M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -156337.99, 38.13, 0 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 2), "3M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -156337.99, 114.53, 0 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 3), "6M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -213862.68, 101.17, 0 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 4), "1Y", 0.0001, RiskFactorKey(), "", 0.0, "USD", -213862.68, 1295.33, -0.12 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "USD", 1), "1M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -156337.99, -37.48, 0 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "USD", 2), "3M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -156337.99, -112.57, 0 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "USD", 3), "6M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -213862.68, -99.35, 0 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "USD", 4), "1Y", 0.0001, RiskFactorKey(), "", 0.0, "USD", -213862.68, -1277.58, 0.11 },
    { "", false, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, RiskFactorKey(), "", 0.0, "USD", -370200.67, -241172.95, 0 }
};
// clang-format on

// Utility function to filter records by trade ID
// The aggregated results have trade ID = "" so do that here for comparison
set<SensitivityRecord> filter(set<SensitivityRecord> records, const string& tradeId) {
    set<SensitivityRecord> res;
    for (auto sr : records) {
        if (sr.tradeId == tradeId) {
            sr.tradeId = "";
            res.insert(sr);
        }
    }

    return res;
}

// Check the expected result, 'exp', against the actual result, 'res'.
void check(const set<SensitivityRecord>& exp, const set<SensitivityRecord>& res) {
    BOOST_CHECK_EQUAL_COLLECTIONS(exp.begin(), exp.end(), res.begin(), res.end());
    for (set<SensitivityRecord>::iterator itExp = exp.begin(), itRes = res.begin();
        itExp != exp.end(); itExp++, itRes++) {
        BOOST_CHECK(QuantLib::close(itExp->baseNpv, itRes->baseNpv));
        BOOST_CHECK(QuantLib::close(itExp->delta, itRes->delta));
        BOOST_CHECK(QuantLib::close(itExp->gamma, itRes->gamma));
    }
}

void SensitivityAggregatorTest::testGeneralAggregationSetCategories() {

    BOOST_TEST_MESSAGE("Testing general aggregation using sets of trades for categories");

    // Streamer
    SensitivityInMemoryStream ss(records);

    // Categories for aggregator
    map<string, set<string>> categories;
    // No aggregation, just single trade categories
    set<string> trades = { "trade_001", "trade_003", "trade_004" };
    for (const auto& trade : trades) {
        categories[trade] = { trade };
    }
    // Aggregate over all trades
    categories["All"] = { "trade_001", "trade_003", "trade_004" };

    // Create aggregator and call aggregate
    SensitivityAggregator sAgg(categories);
    sAgg.aggregate(ss);

    // Containers for expected and actual results respectively below
    set<SensitivityRecord> exp;
    set<SensitivityRecord> res;

    // Test results for single trade categories
    for (const auto& trade : trades) {
        exp = filter(records, trade);
        res = sAgg.sensitivities(trade);
        BOOST_TEST_MESSAGE("Testing for category with single trade " << trade);
        check(exp, res);
    }

    // Test results for the aggregated "All" category
    BOOST_TEST_MESSAGE("Testing for category 'All'");
    res = sAgg.sensitivities("All");
    check(expAggregationAll, res);
}

test_suite* SensitivityAggregatorTest::suite() {
    
    test_suite* suite = BOOST_TEST_SUITE("SensitivityAggregatorTests");

    suite->add(BOOST_TEST_CASE(&SensitivityAggregatorTest::testGeneralAggregationSetCategories));

    return suite;
}

}

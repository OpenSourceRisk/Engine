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

void SensitivityAggregatorTest::testSimpleAggregation() {

    BOOST_TEST_MESSAGE("Testing simple aggregation over three trades");

    // Sensitivity records for aggregation
    set<SensitivityRecord> records = {
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

    // Streamer
    SensitivityInMemoryStream ss(records);

    // Categories for aggregator
    map<string, set<string>> categories;
    // No aggregation, just trade_001
    categories["trade_001"] = { "trade_001" };
    // No aggregation, just trade_003
    categories["trade_003"] = { "trade_003" };
    // No aggregation, just trade_004
    categories["trade_004"] = { "trade_004" };
    // Aggregate over all trades
    categories["All"] = { "trade_001", "trade_003", "trade_004" };

    // Create aggregator and call aggregate
    SensitivityAggregator sAgg(categories);
    sAgg.aggregate(ss);

    // Test results for trade_001 category - should be no aggregation
    set<SensitivityRecord> exp = filter(records, "trade_001");
    set<SensitivityRecord> res = sAgg.sensitivities("trade_001");
    BOOST_CHECK_EQUAL_COLLECTIONS(exp.begin(), exp.end(), res.begin(), res.end());
    for (set<SensitivityRecord>::iterator itExp = exp.begin(), itRes = res.begin(); 
        itExp != exp.end(); itExp++, itRes++) {
        BOOST_CHECK(QuantLib::close(itExp->baseNpv, itRes->baseNpv));
        BOOST_CHECK(QuantLib::close(itExp->delta, itRes->delta));
        BOOST_CHECK(QuantLib::close(itExp->gamma, itRes->gamma));
    }

    // Test results for trade_003 category - should be no aggregation
    exp = filter(records, "trade_003");
    res = sAgg.sensitivities("trade_003");
    BOOST_CHECK_EQUAL_COLLECTIONS(exp.begin(), exp.end(), res.begin(), res.end());
    for (set<SensitivityRecord>::iterator itExp = exp.begin(), itRes = res.begin();
        itExp != exp.end(); itExp++, itRes++) {
        BOOST_CHECK(QuantLib::close(itExp->baseNpv, itRes->baseNpv));
        BOOST_CHECK(QuantLib::close(itExp->delta, itRes->delta));
        BOOST_CHECK(QuantLib::close(itExp->gamma, itRes->gamma));
    }

    // Test results for trade_004 category - should be no aggregation
    exp = filter(records, "trade_004");
    res = sAgg.sensitivities("trade_004");
    BOOST_CHECK_EQUAL_COLLECTIONS(exp.begin(), exp.end(), res.begin(), res.end());
    for (set<SensitivityRecord>::iterator itExp = exp.begin(), itRes = res.begin();
        itExp != exp.end(); itExp++, itRes++) {
        BOOST_CHECK(QuantLib::close(itExp->baseNpv, itRes->baseNpv));
        BOOST_CHECK(QuantLib::close(itExp->delta, itRes->delta));
        BOOST_CHECK(QuantLib::close(itExp->gamma, itRes->gamma));
    }

    // Expected results for the aggregated "All" category
    set<SensitivityRecord> expAll = {
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

    // Test results for the aggregated "All" category
    res = sAgg.sensitivities("All");

    // Check the number and ordering of results
    BOOST_CHECK_EQUAL(res.size(), expAll.size());
    BOOST_CHECK_EQUAL_COLLECTIONS(expAll.begin(), expAll.end(), res.begin(), res.end());
    // Check each of the aggregated records
    for (set<SensitivityRecord>::iterator itExp = expAll.begin(), itRes = res.begin();
        itExp != expAll.end(); itExp++, itRes++) {
        BOOST_CHECK(QuantLib::close(itExp->baseNpv, itRes->baseNpv));
        BOOST_CHECK(QuantLib::close(itExp->delta, itRes->delta));
        BOOST_CHECK(QuantLib::close(itExp->gamma, itRes->gamma));
    }
}

test_suite* SensitivityAggregatorTest::suite() {
    
    test_suite* suite = BOOST_TEST_SUITE("SensitivityAggregatorTests");

    suite->add(BOOST_TEST_CASE(&SensitivityAggregatorTest::testSimpleAggregation));

    return suite;
}

}

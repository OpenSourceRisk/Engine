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
#include <orea/engine/sensitivityaggregator.hpp>
#include <orea/engine/sensitivityinmemorystream.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/math/comparison.hpp>
#include <test/oreatoplevelfixture.hpp>

using namespace boost::unit_test_framework;
using namespace std;

using ore::analytics::RiskFactorKey;
using ore::analytics::SensitivityAggregator;
using ore::analytics::SensitivityInMemoryStream;
using ore::analytics::SensitivityRecord;
using std::function;
using std::map;
using std::set;

using RFType = RiskFactorKey::KeyType;

// Sensitivity records for aggregation
// clang-format off
static const set<SensitivityRecord> records = {
    { "trade_001", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 3), "6M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -103053.46, 74.06, 0.00},
    { "trade_001", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 4), "1Y", 0.0001, RiskFactorKey(), "", 0.0, "USD", -103053.46, 354.79, -0.03 },
    { "trade_001", false, RiskFactorKey(RFType::DiscountCurve, "USD", 3), "6M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -103053.46, -72.54, 0.00 },
    { "trade_001", false, RiskFactorKey(RFType::DiscountCurve, "USD", 4), "1Y", 0.0001, RiskFactorKey(), "", 0.0, "USD", -103053.46, -347.52, 0.02 },
    { "trade_001", false, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, RiskFactorKey(), "", 0.0, "USD", -103053.46, -50331.89, 0.00 },
    { "trade_001", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 3), "6M", 0.0001, RiskFactorKey(RFType::DiscountCurve, "CNY", 4), "1Y", 0.0001, "USD", -103053.46, 0, -0.01 },
    { "trade_001", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 3), "6M", 0.0001, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, "USD", -103053.46, 0, 0.74 },
    { "trade_001", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 4), "1Y", 0.0001, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, "USD", -103053.46, 0, 3.55 },
    { "trade_001", false, RiskFactorKey(RFType::DiscountCurve, "USD", 3), "6M", 0.0001, RiskFactorKey(RFType::DiscountCurve, "USD", 4), "1Y", 0.0001, "USD", -103053.46, 0, 0.01 },
    { "trade_002", false, RiskFactorKey(RFType::DiscountCurve, "TWD", 1), "1M", 0.0001, RiskFactorKey(), "", 0.0, "USD", 393612.36, 0.26, 0.00 },
    { "trade_002", false, RiskFactorKey(RFType::DiscountCurve, "TWD", 2), "3M", 0.0001, RiskFactorKey(), "", 0.0, "USD", 393612.36, 14.11, 0.00 },
    { "trade_002", false, RiskFactorKey(RFType::DiscountCurve, "USD", 1), "1M", 0.0001, RiskFactorKey(), "", 0.0, "USD", 393612.36, -0.43, 0.00 },
    { "trade_002", false, RiskFactorKey(RFType::DiscountCurve, "USD", 2), "3M", 0.0001, RiskFactorKey(), "", 0.0, "USD", 393612.36, -23.32, 0.00 },
    { "trade_002", false, RiskFactorKey(RFType::FXSpot, "TWDUSD", 0), "spot", 0.0002, RiskFactorKey(), "", 0.0, "USD", 393612.36, -6029.41, 0.00 },
    { "trade_003", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 1), "1M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -156337.99, 38.13, 0.00 },
    { "trade_003", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 2), "3M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -156337.99, 114.53, 0.00 },
    { "trade_003", false, RiskFactorKey(RFType::DiscountCurve, "USD", 1), "1M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -156337.99, -37.48, 0.00 },
    { "trade_003", false, RiskFactorKey(RFType::DiscountCurve, "USD", 2), "3M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -156337.99, -112.57, 0.00 },
    { "trade_003", false, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, RiskFactorKey(), "", 0.0, "USD", -156337.99, -91345.92, 0.00 },
    { "trade_003", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 1), "1M", 0.0001, RiskFactorKey(RFType::DiscountCurve, "CNY", 2), "3M", 0.0001, "USD", -156337.99, 0, 0.00 },
    { "trade_003", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 1), "1M", 0.0001, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, "USD", -156337.99, 0, 0.38 },
    { "trade_003", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 2), "3M", 0.0001, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, "USD", -156337.99, 0, 1.15 },
    { "trade_003", false, RiskFactorKey(RFType::DiscountCurve, "USD", 1), "1M", 0.0001, RiskFactorKey(RFType::DiscountCurve, "USD", 2), "3M", 0.0001, "USD", -156337.99, 0, 0.00 },
    { "trade_004", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 3), "6M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -110809.22, 27.11, 0.00 },
    { "trade_004", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 4), "1Y", 0.0001, RiskFactorKey(), "", 0.0, "USD", -110809.22, 940.54, -0.09 },
    { "trade_004", false, RiskFactorKey(RFType::DiscountCurve, "USD", 3), "6M", 0.0001, RiskFactorKey(), "", 0.0, "USD", -110809.22, -26.81, 0.00 },
    { "trade_004", false, RiskFactorKey(RFType::DiscountCurve, "USD", 4), "1Y", 0.0001, RiskFactorKey(), "", 0.0, "USD", -110809.22, -930.06, 0.09 },
    { "trade_004", false, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, RiskFactorKey(), "", 0.0, "USD", -110809.22, -99495.14, 0.00 },
    { "trade_004", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 3), "6M", 0.0001, RiskFactorKey(RFType::DiscountCurve, "CNY", 4), "1Y", 0.0001, "USD", -110809.22, 0, 0.00 },
    { "trade_004", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 3), "6M", 0.0001, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, "USD", -110809.22, 0, 0.27 },
    { "trade_004", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 4), "1Y", 0.0001, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, "USD", -110809.22, 0, 9.41 },
    { "trade_004", false, RiskFactorKey(RFType::DiscountCurve, "USD", 3), "6M", 0.0001, RiskFactorKey(RFType::DiscountCurve, "USD", 4), "1Y", 0.0001, "USD", -110809.22, 0, 0.00 },
    { "trade_005", false, RiskFactorKey(RFType::DiscountCurve, "TWD", 1), "1M", 0.0001, RiskFactorKey(), "", 0.0, "EUR", 393612.36, 0.26, 0.00 },
    { "trade_005", false, RiskFactorKey(RFType::DiscountCurve, "TWD", 2), "3M", 0.0001, RiskFactorKey(), "", 0.0, "EUR", 393612.36, 14.11, 0.00 },
    { "trade_005", false, RiskFactorKey(RFType::DiscountCurve, "USD", 1), "1M", 0.0001, RiskFactorKey(), "", 0.0, "EUR", 393612.36, -0.43, 0.00 },
    { "trade_005", false, RiskFactorKey(RFType::DiscountCurve, "USD", 2), "3M", 0.0001, RiskFactorKey(), "", 0.0, "EUR", 393612.36, -23.32, 0.00 },
    { "trade_005", false, RiskFactorKey(RFType::FXSpot, "TWDUSD", 0), "spot", 0.0002, RiskFactorKey(), "", 0.0, "EUR", 393612.36, -6029.41, 0.00 },
    { "trade_006", false, RiskFactorKey(RFType::DiscountCurve, "TWD", 1), "1M", 0.0001, RiskFactorKey(), "", 0.0, "EUR", -393612.36, -0.26, 0.00 },
    { "trade_006", false, RiskFactorKey(RFType::DiscountCurve, "TWD", 2), "3M", 0.0001, RiskFactorKey(), "", 0.0, "EUR", -393612.36, -14.11, 0.00 },
    { "trade_006", false, RiskFactorKey(RFType::DiscountCurve, "USD", 1), "1M", 0.0001, RiskFactorKey(), "", 0.0, "EUR", -393612.36, 0.43, 0.00 },
    { "trade_006", false, RiskFactorKey(RFType::DiscountCurve, "USD", 2), "3M", 0.0001, RiskFactorKey(), "", 0.0, "EUR", -393612.36, 23.32, 0.00 },
    { "trade_006", false, RiskFactorKey(RFType::FXSpot, "TWDUSD", 0), "spot", 0.0002, RiskFactorKey(), "", 0.0, "EUR", -393612.36, 6029.41, 0.00 }
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
    { "", false, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, RiskFactorKey(), "", 0.0, "USD", -370200.67, -241172.95, 0 }, 
    { "", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 1), "1M", 0.0001, RiskFactorKey(RFType::DiscountCurve, "CNY", 2), "3M", 0.0001, "USD", -156337.99, 0.00, 0.00 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 1), "1M", 0.0001, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, "USD", -156337.99, 0.00, 0.38 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 2), "3M", 0.0001, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, "USD", -156337.99, 0.00, 1.15 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 3), "6M", 0.0001, RiskFactorKey(RFType::DiscountCurve, "CNY", 4), "1Y", 0.0001, "USD", -213862.68, 0.00, -0.01 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 3), "6M", 0.0001, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, "USD", -213862.68, 0.00, 1.01 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "CNY", 4), "1Y", 0.0001, RiskFactorKey(RFType::FXSpot, "CNYUSD", 0), "spot", 0.001534, "USD", -213862.68, 0.00, 12.96 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "USD", 1), "1M", 0.0001, RiskFactorKey(RFType::DiscountCurve, "USD", 2), "3M", 0.0001, "USD", -156337.99, 0.00, 0.00 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "USD", 3), "6M", 0.0001, RiskFactorKey(RFType::DiscountCurve, "USD", 4), "1Y", 0.0001, "USD", -213862.68, 0.00, 0.01 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "TWD", 1), "1M", 0.0001, RiskFactorKey(), "", 0.0, "EUR", 0, 0, 0.00 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "TWD", 2), "3M", 0.0001, RiskFactorKey(), "", 0.0, "EUR", 0, 0, 0.00 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "USD", 1), "1M", 0.0001, RiskFactorKey(), "", 0.0, "EUR", 0, 0, 0.00 },
    { "", false, RiskFactorKey(RFType::DiscountCurve, "USD", 2), "3M", 0.0001, RiskFactorKey(), "", 0.0, "EUR", 0, 0, 0.00 },
    { "", false, RiskFactorKey(RFType::FXSpot, "TWDUSD", 0), "spot", 0.0002, RiskFactorKey(), "", 0.0, "EUR", 0, 0, 0.00 }
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
void check(const set<SensitivityRecord>& exp, const set<SensitivityRecord>& res, const string& category) {
    BOOST_CHECK_EQUAL_COLLECTIONS(exp.begin(), exp.end(), res.begin(), res.end());
    for (set<SensitivityRecord>::iterator itExp = exp.begin(), itRes = res.begin(); itExp != exp.end();
         itExp++, itRes++) {
        BOOST_CHECK_MESSAGE(QuantLib::close(itExp->baseNpv, itRes->baseNpv),
                            "Category = " << category << ": Base NPVs (exp = " << itExp->baseNpv
                                          << " vs. actual = " << itRes->baseNpv
                                          << ") are not equal for aggregated expected record " << *itExp);
        BOOST_CHECK_MESSAGE(QuantLib::close(itExp->delta, itRes->delta),
                            "Category = " << category << ": Deltas (exp = " << itExp->delta
                                          << " vs. actual = " << itRes->delta
                                          << ") are not equal for aggregated expected record " << *itExp);
        BOOST_CHECK_MESSAGE(QuantLib::close(itExp->gamma, itRes->gamma),
                            "Category = " << category << ": Gammas (exp = " << itExp->gamma
                                          << " vs. actual = " << itRes->gamma
                                          << ") are not equal for aggregated expected record " << *itExp);
    }
}

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(SensitivityAggregatorTest)

BOOST_AUTO_TEST_CASE(testGeneralAggregationSetCategories) {

    BOOST_TEST_MESSAGE("Testing general aggregation using sets of trades for categories");

    // Streamer
    SensitivityInMemoryStream ss(records.begin(), records.end());

    // Categories for aggregator
    map<string, set<std::pair<std::string, QuantLib::Size>>> categories;
    // No aggregation, just single trade categories
    set<pair<string, QuantLib::Size>> trades = {make_pair("trade_001", 0), make_pair("trade_003", 1),
                                                make_pair("trade_004", 2), make_pair("trade_005", 3),
                                                make_pair("trade_006", 4)};

    for (const auto& trade : trades) {
        categories[trade.first] = {trade};
    }
    // Aggregate over all trades except trade_002
    categories["all_except_002"] = trades;

    // Create aggregator and call aggregate
    SensitivityAggregator sAgg(categories);
    sAgg.aggregate(ss);

    // Containers for expected and actual results respectively below
    set<SensitivityRecord> exp;
    set<SensitivityRecord> res;

    // Test results for single trade categories
    for (const auto& trade : trades) {
        exp = filter(records, trade.first);
        res = sAgg.sensitivities(trade.first);
        BOOST_TEST_MESSAGE("Testing for category with single trade " << trade.first);
        check(exp, res, trade.first);
    }

    // Test results for the aggregated "All" category
    BOOST_TEST_MESSAGE("Testing for category 'all_except_002'");
    res = sAgg.sensitivities("all_except_002");
    check(expAggregationAll, res, "all_except_002");
}

BOOST_AUTO_TEST_CASE(testGeneralAggregationFunctionCategories) {

    BOOST_TEST_MESSAGE("Testing general aggregation using functions for categories");

    // Streamer
    SensitivityInMemoryStream ss(records.begin(), records.end());

    // Category functions for aggregator
    map<string, function<bool(string)>> categories;
    // No aggregation, just single trade categories
    set<pair<string, QuantLib::Size>> trades = {make_pair("trade_001", 0), make_pair("trade_003", 1),
                                                make_pair("trade_004", 2), make_pair("trade_005", 3),
                                                make_pair("trade_006", 4)};

    for (const auto& trade : trades) {
        categories[trade.first] = [&trade](string tradeId) { return tradeId == trade.first; };
    }
    // Aggregate over all trades except trade_002
    categories["all_except_002"] = [&trades](string tradeId) {
        for (auto it = trades.begin(); it != trades.end(); ++it) {
            if (it->first == tradeId)
                return true;
        }
        return false;
    };

    // Create aggregator and call aggregate
    SensitivityAggregator sAgg(categories);
    sAgg.aggregate(ss);

    // Containers for expected and actual results respectively below
    set<SensitivityRecord> exp;
    set<SensitivityRecord> res;

    // Test results for single trade categories
    for (const auto& trade : trades) {
        exp = filter(records, trade.first);
        res = sAgg.sensitivities(trade.first);
        BOOST_TEST_MESSAGE("Testing for category with single trade " << trade.first);
        check(exp, res, trade.first);
    }

    // Test results for the aggregated "All" category
    BOOST_TEST_MESSAGE("Testing for category 'all_except_002'");
    res = sAgg.sensitivities("all_except_002");
    check(expAggregationAll, res, "all_except_002");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

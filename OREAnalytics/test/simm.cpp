/*
 Copyright (C) 2016, 2017 Quaternion Risk Management Ltd.
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

#include <oret/toplevelfixture.hpp>
#include <test/oreatoplevelfixture.hpp>
#include <boost/make_shared.hpp>
#include <ored/marketdata/dummymarket.hpp>
#include <orea/simm/crifloader.hpp>
#include <orea/simm/simmbucketmapperbase.hpp>
#include <orea/simm/simmcalculator.hpp>
#include <orea/simm/simmconfigurationisdav1_0.hpp>
#include <orea/simm/simmconfigurationisdav1_3.hpp>
#include <orea/simm/simmresults.hpp>

using namespace ore::data;
using namespace ore::analytics;
using namespace boost::unit_test_framework;
using std::map;

using QuantLib::Real;

// Ease notation again
typedef SimmConfiguration::ProductClass ProductClass;
typedef SimmConfiguration::RiskClass RiskClass;
typedef SimmConfiguration::MarginType MarginType;
typedef SimmConfiguration::RiskType RiskType;
typedef SimmConfiguration::SimmSide SimmSide;

namespace {

void testIrDeltaMargin(const boost::shared_ptr<SimmConfiguration>& config, const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM IR Delta (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::IRCurve;
    ProductClass pc = ProductClass::RatesFX;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "BRL", "3", "1y", "Libor1m", "USD", 1023.45, 1023.45 });
    cl.add({ "trade_02", "", "pf", pc, rt, "BRL", "3", "2y", "Libor1m", "USD", 1024.45, 1024.45 });
    cl.add({ "trade_03", "", "pf", pc, rt, "BRL", "3", "5y", "Libor1m", "USD", -1025.45, -1025.45 });
    cl.add({ "trade_04", "", "pf", pc, rt, "BRL", "3", "10y", "Libor1m", "USD", 1026.45, 1026.45 });
    cl.add({ "trade_05", "", "pf", pc, rt, "BRL", "3", "1y", "Libor3m", "USD", -1027.45, -1027.45 });
    cl.add({ "trade_06", "", "pf", pc, rt, "BRL", "3", "2y", "Libor3m", "USD", 1028.45, 1028.45 });
    cl.add({ "trade_07", "", "pf", pc, rt, "BRL", "3", "5y", "Libor3m", "USD", 1029.45, 1029.45 });
    cl.add({ "trade_08", "", "pf", pc, rt, "BRL", "3", "10y", "Libor3m", "USD", 1030.45, 1030.45 });
    cl.add({ "trade_09", "", "pf", pc, rt, "BRL", "3", "1y", "Libor6m", "USD", -1031.45, -1031.45 });
    cl.add({ "trade_10", "", "pf", pc, rt, "BRL", "3", "2y", "Libor6m", "USD", -1032.45, -1032.45 });
    cl.add({ "trade_11", "", "pf", pc, rt, "BRL", "3", "5y", "Libor6m", "USD", 1033.45, 1033.45 });
    cl.add({ "trade_12", "", "pf", pc, rt, "BRL", "3", "10y", "Libor6m", "USD", 1034.45, 1034.45 });
    cl.add({ "trade_13", "", "pf", pc, rt, "BRL", "3", "1y", "Libor12m", "USD", -1035.45, -1035.45 });
    cl.add({ "trade_14", "", "pf", pc, rt, "BRL", "3", "2y", "Libor12m", "USD", 1036.45, 1036.45 });
    cl.add({ "trade_15", "", "pf", pc, rt, "BRL", "3", "5y", "Libor12m", "USD", -1037.45, -1037.45 });
    cl.add({ "trade_16", "", "pf", pc, rt, "BRL", "3", "10y", "Libor12m", "USD", 1038.45, 1038.45 });
    cl.add({ "trade_17", "", "pf", pc, rt, "JPY", "2", "1y", "Libor1m", "USD", 1039.45, 1039.45 });
    cl.add({ "trade_18", "", "pf", pc, rt, "JPY", "2", "2y", "Libor1m", "USD", -1040.45, -1040.45 });
    cl.add({ "trade_19", "", "pf", pc, rt, "JPY", "2", "5y", "Libor1m", "USD", -1041.45, -1041.45 });
    cl.add({ "trade_20", "", "pf", pc, rt, "JPY", "2", "10y", "Libor1m", "USD", -1042.45, -1042.45 });
    cl.add({ "trade_21", "", "pf", pc, rt, "JPY", "2", "1y", "Libor3m", "USD", 1043.45, 1043.45 });
    cl.add({ "trade_22", "", "pf", pc, rt, "JPY", "2", "2y", "Libor3m", "USD", -1044.45, -1044.45 });
    cl.add({ "trade_23", "", "pf", pc, rt, "JPY", "2", "5y", "Libor3m", "USD", 1045.45, 1045.45 });
    cl.add({ "trade_24", "", "pf", pc, rt, "JPY", "2", "10y", "Libor3m", "USD", -1046.45, -1046.45 });
    cl.add({ "trade_25", "", "pf", pc, rt, "JPY", "2", "1y", "Libor6m", "USD", 1047.45, 1047.45 });
    cl.add({ "trade_26", "", "pf", pc, rt, "JPY", "2", "2y", "Libor6m", "USD", 1048.45, 1048.45 });
    cl.add({ "trade_27", "", "pf", pc, rt, "JPY", "2", "5y", "Libor6m", "USD", -1049.45, -1049.45 });
    cl.add({ "trade_28", "", "pf", pc, rt, "JPY", "2", "10y", "Libor6m", "USD", -1050.45, -1050.45 });
    cl.add({ "trade_29", "", "pf", pc, rt, "JPY", "2", "1y", "Libor12m", "USD", 1051.45, 1051.45 });
    cl.add({ "trade_30", "", "pf", pc, rt, "JPY", "2", "2y", "Libor12m", "USD", -1052.45, -1052.45 });
    cl.add({ "trade_31", "", "pf", pc, rt, "JPY", "2", "5y", "Libor12m", "USD", 1053.45, 1053.45 });
    cl.add({ "trade_32", "", "pf", pc, rt, "JPY", "2", "10y", "Libor12m", "USD", 1053.45, 1053.45 });
    cl.add({ "trade_33", "", "pf", pc, rt, "USD", "1", "1y", "Libor1m", "USD", -1053.45, -1053.45 });
    cl.add({ "trade_34", "", "pf", pc, rt, "USD", "1", "2y", "Libor1m", "USD", -1053.45, -1053.45 });
    cl.add({ "trade_35", "", "pf", pc, rt, "USD", "1", "5y", "Libor1m", "USD", 1053.45, 1053.45 });
    cl.add({ "trade_36", "", "pf", pc, rt, "USD", "1", "10y", "Libor1m", "USD", -1053.45, -1053.45 });
    cl.add({ "trade_37", "", "pf", pc, rt, "USD", "1", "1y", "Libor3m", "USD", 1053.45, 1053.45 });
    cl.add({ "trade_38", "", "pf", pc, rt, "USD", "1", "2y", "Libor3m", "USD", 1053.45, 1053.45 });
    cl.add({ "trade_39", "", "pf", pc, rt, "USD", "1", "5y", "Libor3m", "USD", -1053.45, -1053.45 });
    cl.add({ "trade_40", "", "pf", pc, rt, "USD", "1", "10y", "Libor3m", "USD", -1053.45, -1053.45 });
    cl.add({ "trade_41", "", "pf", pc, rt, "USD", "1", "1y", "Libor6m", "USD", 1053.45, 1053.45 });
    cl.add({ "trade_42", "", "pf", pc, rt, "USD", "1", "2y", "Libor6m", "USD", 1053.45, 1053.45 });
    cl.add({ "trade_43", "", "pf", pc, rt, "USD", "1", "5y", "Libor6m", "USD", 1053.45, 1053.45 });
    cl.add({ "trade_44", "", "pf", pc, rt, "USD", "1", "10y", "Libor6m", "USD", -1053.45, -1053.45 });
    cl.add({ "trade_45", "", "pf", pc, rt, "USD", "1", "1y", "Libor12m", "USD", 1053.45, 1053.45 });
    cl.add({ "trade_46", "", "pf", pc, rt, "USD", "1", "2y", "Libor12m", "USD", 1053.45, 1053.45 });
    cl.add({ "trade_47", "", "pf", pc, rt, "USD", "1", "5y", "Libor12m", "USD", 1053.45, 1053.45 });
    cl.add({ "trade_48", "", "pf", pc, rt, "USD", "1", "10y", "Libor12m", "USD", 1053.45, 1053.45 });
    // clang-format on

    RiskClass rc = RiskClass::InterestRate;
    MarginType mt = MarginType::Delta;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 491936.667626566;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify IR Delta Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                  << ", expected " << expected << ", difference "
                                                                  << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testFxDeltaMargin(const boost::shared_ptr<SimmConfiguration>& config, const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM FX Delta (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::FX;
    ProductClass pc = ProductClass::RatesFX;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "IDR", "", "", "", "USD", 5402.350999, 5402.350999 });
    cl.add({ "trade_02", "", "pf", pc, rt, "JPY", "", "", "", "USD", -34390.56314, -34390.56314 });
    cl.add({ "trade_03", "", "pf", pc, rt, "USD", "", "", "", "USD", 2254.604708, 2254.604708 });
    // clang-format on

    RiskClass rc = RiskClass::FX;
    MarginType mt = MarginType::Delta;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 253059.867316875;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify FX Delta Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                  << ", expected " << expected << ", difference "
                                                                  << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testCrqDeltaMargin(const boost::shared_ptr<SimmConfiguration>& config, const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM CRQ Delta (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::CreditQ;
    ProductClass pc = ProductClass::Credit;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "Issuer 1", "1", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_02", "", "pf", pc, rt, "Issuer 1", "1", "1y", "", "USD", 8.059730786, 8.059730786 });
    cl.add({ "trade_03", "", "pf", pc, rt, "Issuer 1", "1", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_04", "", "pf", pc, rt, "Issuer 1", "1", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_05", "", "pf", pc, rt, "Issuer 1", "1", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_06", "", "pf", pc, rt, "Issuer 2", "2", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_07", "", "pf", pc, rt, "Issuer 2", "2", "1y", "", "USD", 3.635153393, 3.635153393 });
    cl.add({ "trade_08", "", "pf", pc, rt, "Issuer 2", "2", "2y", "", "USD", 4.07343881, 4.07343881 });
    cl.add({ "trade_09", "", "pf", pc, rt, "Issuer 2", "2", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_10", "", "pf", pc, rt, "Issuer 2", "2", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_11", "", "pf", pc, rt, "Issuer 3", "3", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_12", "", "pf", pc, rt, "Issuer 3", "3", "1y", "", "USD", 580.6019555, 580.6019555 });
    cl.add({ "trade_13", "", "pf", pc, rt, "Issuer 3", "3", "2y", "", "USD", 5078.479979, 5078.479979 });
    cl.add({ "trade_14", "", "pf", pc, rt, "Issuer 3", "3", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_15", "", "pf", pc, rt, "Issuer 3", "3", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_16", "", "pf", pc, rt, "Issuer 4", "4", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_17", "", "pf", pc, rt, "Issuer 4", "4", "1y", "", "USD", -70.1134237, -70.1134237 });
    cl.add({ "trade_18", "", "pf", pc, rt, "Issuer 4", "4", "2y", "", "USD", -36.92112038, -36.92112038 });
    cl.add({ "trade_19", "", "pf", pc, rt, "Issuer 4", "4", "3y", "", "USD", -2237.406338, -2237.406338 });
    cl.add({ "trade_20", "", "pf", pc, rt, "Issuer 4", "4", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_21", "", "pf", pc, rt, "Issuer 5", "5", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_22", "", "pf", pc, rt, "Issuer 5", "5", "1y", "", "USD", 4.289346749, 4.289346749 });
    cl.add({ "trade_23", "", "pf", pc, rt, "Issuer 5", "5", "2y", "", "USD", 14.13859239, 14.13859239 });
    cl.add({ "trade_24", "", "pf", pc, rt, "Issuer 5", "5", "3y", "", "USD", 1345.479615, 1345.479615 });
    cl.add({ "trade_25", "", "pf", pc, rt, "Issuer 5", "5", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_26", "", "pf", pc, rt, "Issuer 6", "6", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_27", "", "pf", pc, rt, "Issuer 6", "6", "1y", "", "USD", 8.508687406, 8.508687406 });
    cl.add({ "trade_28", "", "pf", pc, rt, "Issuer 6", "6", "2y", "", "USD", 20.53329364, 20.53329364 });
    cl.add({ "trade_29", "", "pf", pc, rt, "Issuer 6", "6", "3y", "", "USD", 404.4754133, 404.4754133 });
    cl.add({ "trade_30", "", "pf", pc, rt, "Issuer 6", "6", "5y", "", "USD", 403.8745725, 403.8745725 });
    cl.add({ "trade_31", "", "pf", pc, rt, "Issuer 7", "7", "10y", "", "USD", 60.55963973, 60.55963973 });
    cl.add({ "trade_32", "", "pf", pc, rt, "Issuer 7", "7", "1y", "", "USD", -1.811958229, -1.811958229 });
    cl.add({ "trade_33", "", "pf", pc, rt, "Issuer 7", "7", "2y", "", "USD", -5.504450405, -5.504450405 });
    cl.add({ "trade_34", "", "pf", pc, rt, "Issuer 7", "7", "3y", "", "USD", -4.260395846, -4.260395846 });
    cl.add({ "trade_35", "", "pf", pc, rt, "Issuer 7", "7", "5y", "", "USD", 474.0116061, 474.0116061 });
    cl.add({ "trade_36", "", "pf", pc, rt, "Issuer 8", "8", "10y", "", "USD", 104.8098969, 104.8098969 });
    cl.add({ "trade_37", "", "pf", pc, rt, "Issuer 8", "8", "1y", "", "USD", -0.097966563, -0.097966563 });
    cl.add({ "trade_38", "", "pf", pc, rt, "Issuer 8", "8", "2y", "", "USD", -0.431121774, -0.431121774 });
    cl.add({ "trade_39", "", "pf", pc, rt, "Issuer 8", "8", "3y", "", "USD", -0.686076784, -0.686076784 });
    cl.add({ "trade_40", "", "pf", pc, rt, "Issuer 8", "8", "5y", "", "USD", 260.6834549, 260.6834549 });
    cl.add({ "trade_41", "", "pf", pc, rt, "Issuer 9", "9", "10y", "", "USD", 134.4598543, 134.4598543 });
    cl.add({ "trade_42", "", "pf", pc, rt, "Issuer 9", "9", "1y", "", "USD", 0.008044421, 0.008044421 });
    cl.add({ "trade_43", "", "pf", pc, rt, "Issuer 9", "9", "2y", "", "USD", 0.013779813, 0.013779813 });
    cl.add({ "trade_44", "", "pf", pc, rt, "Issuer 9", "9", "3y", "", "USD", 0.147860763, 0.147860763 });
    cl.add({ "trade_45", "", "pf", pc, rt, "Issuer 9", "9", "5y", "", "USD", 683.9072321, 683.9072321 });
    cl.add({ "trade_46", "", "pf", pc, rt, "Issuer 10", "10", "10y", "", "USD", 122.1352924, 122.1352924 });
    cl.add({ "trade_47", "", "pf", pc, rt, "Issuer 10", "10", "1y", "", "USD", 0.069530089, 0.069530089 });
    cl.add({ "trade_48", "", "pf", pc, rt, "Issuer 10", "10", "2y", "", "USD", 0.307621389, 0.307621389 });
    cl.add({ "trade_49", "", "pf", pc, rt, "Issuer 10", "10", "3y", "", "USD", 1.073502362, 1.073502362 });
    cl.add({ "trade_50", "", "pf", pc, rt, "Issuer 10", "10", "5y", "", "USD", 561.9736274, 561.9736274 });
    cl.add({ "trade_51", "", "pf", pc, rt, "Issuer 11", "11", "10y", "", "USD", 128.7909159, 128.7909159 });
    cl.add({ "trade_52", "", "pf", pc, rt, "Issuer 11", "11", "1y", "", "USD", 0.179342208, 0.179342208 });
    cl.add({ "trade_53", "", "pf", pc, rt, "Issuer 11", "11", "2y", "", "USD", 0.142506059, 0.142506059 });
    cl.add({ "trade_54", "", "pf", pc, rt, "Issuer 11", "11", "3y", "", "USD", 0.253435337, 0.253435337 });
    cl.add({ "trade_55", "", "pf", pc, rt, "Issuer 11", "11", "5y", "", "USD", 160.1397076, 160.1397076 });
    cl.add({ "trade_56", "", "pf", pc, rt, "Issuer 12", "12", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_57", "", "pf", pc, rt, "Issuer 12", "12", "1y", "", "USD", -0.054311349, -0.054311349 });
    cl.add({ "trade_58", "", "pf", pc, rt, "Issuer 12", "12", "2y", "", "USD", -0.065199114, -0.065199114 });
    cl.add({ "trade_59", "", "pf", pc, rt, "Issuer 12", "12", "3y", "", "USD", 121.3343297, 121.3343297 });
    cl.add({ "trade_60", "", "pf", pc, rt, "Issuer 12", "12", "5y", "", "USD", 227.1665079, 227.1665079 });
    cl.add({ "trade_61", "", "pf", pc, rt, "Issuer 13", "Residual", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_62", "", "pf", pc, rt, "Issuer 13", "Residual", "1y", "", "USD", 2.50268281, 2.50268281 });
    cl.add({ "trade_63", "", "pf", pc, rt, "Issuer 13", "Residual", "2y", "", "USD", 92.21211014, 92.21211014 });
    cl.add({ "trade_64", "", "pf", pc, rt, "Issuer 13", "Residual", "3y", "", "USD", 1759.025026, 1759.025026 });
    cl.add({ "trade_65", "", "pf", pc, rt, "Issuer 13", "Residual", "5y", "", "USD", 0, 0 });
    // clang-format on

    RiskClass rc = RiskClass::CreditQualifying;
    MarginType mt = MarginType::Delta;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 2079261.791598740;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify CRQ Delta Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                   << ", expected " << expected << ", difference "
                                                                   << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testCrnqDeltaMargin(const boost::shared_ptr<SimmConfiguration>& config, const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM CRNQ Delta (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::CreditNonQ;
    ProductClass pc = ProductClass::Credit;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "Issuer 1", "1", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_02", "", "pf", pc, rt, "Issuer 1", "1", "1y", "", "USD", -1544.867056, -1544.867056 });
    cl.add({ "trade_03", "", "pf", pc, rt, "Issuer 1", "1", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_04", "", "pf", pc, rt, "Issuer 1", "1", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_05", "", "pf", pc, rt, "Issuer 1", "1", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_06", "", "pf", pc, rt, "Issuer 2", "Residual", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_07", "", "pf", pc, rt, "Issuer 2", "Residual", "1y", "", "USD", -1231.475557, -1231.475557 });
    cl.add({ "trade_08", "", "pf", pc, rt, "Issuer 2", "Residual", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_09", "", "pf", pc, rt, "Issuer 2", "Residual", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_10", "", "pf", pc, rt, "Issuer 2", "Residual", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_11", "", "pf", pc, rt, "Issuer 3", "1", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_12", "", "pf", pc, rt, "Issuer 3", "1", "1y", "", "USD", 0, 0 });
    cl.add({ "trade_13", "", "pf", pc, rt, "Issuer 3", "1", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_14", "", "pf", pc, rt, "Issuer 3", "1", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_15", "", "pf", pc, rt, "Issuer 3", "1", "5y", "", "USD", 15205.55176, 15205.55176 });
    cl.add({ "trade_16", "", "pf", pc, rt, "Issuer 4", "Residual", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_17", "", "pf", pc, rt, "Issuer 4", "Residual", "1y", "", "USD", 0, 0 });
    cl.add({ "trade_18", "", "pf", pc, rt, "Issuer 4", "Residual", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_19", "", "pf", pc, rt, "Issuer 4", "Residual", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_20", "", "pf", pc, rt, "Issuer 4", "Residual", "5y", "", "USD", 169.78, 169.78 });
    cl.add({ "trade_21", "", "pf", pc, rt, "Issuer 5", "2", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_22", "", "pf", pc, rt, "Issuer 5", "2", "1y", "", "USD", 1867.51, 1867.51 });
    cl.add({ "trade_23", "", "pf", pc, rt, "Issuer 5", "2", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_24", "", "pf", pc, rt, "Issuer 5", "2", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_25", "", "pf", pc, rt, "Issuer 5", "2", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_26", "", "pf", pc, rt, "Issuer 6", "2", "10y", "", "USD", -784.24, -784.24 });
    cl.add({ "trade_27", "", "pf", pc, rt, "Issuer 6", "2", "1y", "", "USD", 0, 0 });
    cl.add({ "trade_28", "", "pf", pc, rt, "Issuer 6", "2", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_29", "", "pf", pc, rt, "Issuer 6", "2", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_30", "", "pf", pc, rt, "Issuer 6", "2", "5y", "", "USD", 0, 0 });
    // clang-format on

    RiskClass rc = RiskClass::CreditNonQualifying;
    MarginType mt = MarginType::Delta;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 5933904.463342030;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify CRNQ Delta Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                    << ", expected " << expected << ", difference "
                                                                    << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testEqDeltaMargin(const boost::shared_ptr<SimmConfiguration>& config, const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM EQ Delta (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::Equity;
    ProductClass pc = ProductClass::Equity;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "Index 1", "1", "", "", "USD", 1730.821481, 1730.821481 });
    cl.add({ "trade_02", "", "pf", pc, rt, "Index 2", "2", "", "", "USD", 613.590721, 613.590721 });
    cl.add({ "trade_03", "", "pf", pc, rt, "Index 3", "3", "", "", "USD", 1426780.043, 1426780.043 });
    cl.add({ "trade_04", "", "pf", pc, rt, "Index 4", "4", "", "", "USD", 31780.2661, 31780.2661 });
    cl.add({ "trade_05", "", "pf", pc, rt, "Index 5", "5", "", "", "USD", -4578.088796, -4578.088796 });
    cl.add({ "trade_06", "", "pf", pc, rt, "Index 6", "6", "", "", "USD", 12640.91897, 12640.91897 });
    cl.add({ "trade_07", "", "pf", pc, rt, "Index 7", "7", "", "", "USD", 19519.81714, 19519.81714 });
    cl.add({ "trade_08", "", "pf", pc, rt, "Index 8", "8", "", "", "USD", -7539.335782, -7539.335782 });
    cl.add({ "trade_09", "", "pf", pc, rt, "Index 9", "9", "", "", "USD", 491.9781852, 491.9781852 });
    cl.add({ "trade_10", "", "pf", pc, rt, "Index 10", "10", "", "", "USD", 2807.153926, 2807.153926 });
    cl.add({ "trade_11", "", "pf", pc, rt, "Index 11", "11", "", "", "USD", 1729088.977, 1729088.977 });
    cl.add({ "trade_12", "", "pf", pc, rt, "Index 12", "Residual", "", "", "USD", -49598.35456, -49598.35456 });
    cl.add({ "trade_13", "", "pf", pc, rt, "Index 13", "1", "", "", "USD", 164027.5537, 164027.5537 });
    cl.add({ "trade_14", "", "pf", pc, rt, "Index 14", "2", "", "", "USD", 25842.70371, 25842.70371 });
    cl.add({ "trade_15", "", "pf", pc, rt, "Index 15", "3", "", "", "USD", -6649.624384, -6649.624384 });
    cl.add({ "trade_16", "", "pf", pc, rt, "Index 16", "4", "", "", "USD", -25668.34679, -25668.34679 });
    cl.add({ "trade_17", "", "pf", pc, rt, "Index 17", "5", "", "", "USD", -4791.677268, -4791.677268 });
    cl.add({ "trade_18", "", "pf", pc, rt, "Index 18", "6", "", "", "USD", 201885.1392, 201885.1392 });
    cl.add({ "trade_19", "", "pf", pc, rt, "Index 19", "7", "", "", "USD", 162156.1828, 162156.1828 });
    cl.add({ "trade_20", "", "pf", pc, rt, "Index 20", "8", "", "", "USD", 37946.32581, 37946.32581 });
    cl.add({ "trade_21", "", "pf", pc, rt, "Index 21", "9", "", "", "USD", -10625.23451, -10625.23451 });
    cl.add({ "trade_22", "", "pf", pc, rt, "Index 22", "10", "", "", "USD", 63432.80115, 63432.80115 });
    cl.add({ "trade_23", "", "pf", pc, rt, "Index 23", "11", "", "", "USD", -80978.91161, -80978.91161 });
    cl.add({ "trade_24", "", "pf", pc, rt, "Index 24", "Residual", "", "", "USD", -13145.44119, -13145.44119 });
    // clang-format on

    RiskClass rc = RiskClass::Equity;
    MarginType mt = MarginType::Delta;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 58026595.421413700;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify EQ Delta Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                  << ", expected " << expected << ", difference "
                                                                  << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testComDeltaMargin(const boost::shared_ptr<SimmConfiguration>& config, const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM COM Delta (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::Commodity;
    ProductClass pc = ProductClass::Commodity;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "Coal Americas", "1", "", "", "USD", -2335.613204, -2335.613204 });
    cl.add({ "trade_02", "", "pf", pc, rt, "Crude oil Americas", "2", "", "", "USD", -23889.50368, -23889.50368 });
    cl.add({ "trade_03", "", "pf", pc, rt, "Light Ends Americas", "3", "", "", "USD", 164027.5537, 164027.5537 });
    cl.add({ "trade_04", "", "pf", pc, rt, "Middle Distillates Americas", "4", "", "", "USD", 25842.70371, 25842.70371 });
    cl.add({ "trade_05", "", "pf", pc, rt, "Heavy Distillates Americas", "5", "", "", "USD", -6649.624384, -6649.624384 });
    cl.add({ "trade_06", "", "pf", pc, rt, "NA Natural Gas Gulf Coast", "6", "", "", "USD", -25668.34679, -25668.34679 });
    cl.add({ "trade_07", "", "pf", pc, rt, "EU Natural Gas Europe", "7", "", "", "USD", -4791.677268, -4791.677268 });
    cl.add({ "trade_08", "", "pf", pc, rt, "NA Power Eastern Interconnect", "8", "", "", "USD", 201885.1392, 201885.1392 });
    cl.add({ "trade_09", "", "pf", pc, rt, "EU Power Germany", "9", "", "", "USD", 162156.1828, 162156.1828 });
    cl.add({ "trade_10", "", "pf", pc, rt, "Freight Wet", "10", "", "", "USD", 37946.32581, 37946.32581 });
    cl.add({ "trade_11", "", "pf", pc, rt, "Base Metals Aluminium", "11", "", "", "USD", -10625.23451, -10625.23451 });
    cl.add({ "trade_12", "", "pf", pc, rt, "Precious Metals Gold", "12", "", "", "USD", 63432.80115, 63432.80115 });
    cl.add({ "trade_13", "", "pf", pc, rt, "Grains Corn", "13", "", "", "USD", -18582.29828, -18582.29828 });
    cl.add({ "trade_14", "", "pf", pc, rt, "Softs Cocoa", "14", "", "", "USD", 21798.4303, 21798.4303 });
    cl.add({ "trade_15", "", "pf", pc, rt, "Livestock Live Cattle", "15", "", "", "USD", -12865.6199, -12865.6199 });
    cl.add({ "trade_16", "", "pf", pc, rt, "Other", "16", "", "", "USD", 42476.68516, 42476.68516 });
    cl.add({ "trade_17", "", "pf", pc, rt, "Coal Europe", "1", "", "", "USD", -80978.91161, -80978.91161 });
    cl.add({ "trade_18", "", "pf", pc, rt, "Crude oil Europe", "2", "", "", "USD", -13145.44119, -13145.44119 });
    cl.add({ "trade_19", "", "pf", pc, rt, "Light Ends Europe", "3", "", "", "USD", 3449.498529, 3449.498529 });
    cl.add({ "trade_20", "", "pf", pc, rt, "Middle Distillates Europe", "4", "", "", "USD", -85285.13009, -85285.13009 });
    cl.add({ "trade_21", "", "pf", pc, rt, "Heavy Distillates Europe", "5", "", "", "USD", -9835.031475, -9835.031475 });
    cl.add({ "trade_22", "", "pf", pc, rt, "NA Natural Gas North East", "6", "", "", "USD", -19211.18697, -19211.18697 });
    cl.add({ "trade_23", "", "pf", pc, rt, "EU Natural Gas Europe", "7", "", "", "USD", 49252.852, 49252.852 });
    cl.add({ "trade_24", "", "pf", pc, rt, "NA Power ERCOT", "8", "", "", "USD", 70674.42089, 70674.42089 });
    cl.add({ "trade_25", "", "pf", pc, rt, "EU Power UK", "9", "", "", "USD", -40550.13604, -40550.13604 });
    cl.add({ "trade_26", "", "pf", pc, rt, "Freight Dry", "10", "", "", "USD", -7791.69971, -7791.69971 });
    cl.add({ "trade_27", "", "pf", pc, rt, "Base Metals Copper", "11", "", "", "USD", -3065.371541, -3065.371541 });
    cl.add({ "trade_28", "", "pf", pc, rt, "Precious Metals Silver", "12", "", "", "USD", 206541.8901, 206541.8901 });
    cl.add({ "trade_29", "", "pf", pc, rt, "Grains Soybeans", "13", "", "", "USD", 8704.175998, 8704.175998 });
    cl.add({ "trade_30", "", "pf", pc, rt, "Softs Coffee", "14", "", "", "USD", -104237.0139, -104237.0139 });
    cl.add({ "trade_31", "", "pf", pc, rt, "Livestock Feeder Cattle", "15", "", "", "USD", -327608.4274, -327608.4274 });
    cl.add({ "trade_32", "", "pf", pc, rt, "Other", "16", "", "", "USD", -21702.70893, -21702.70893 });
    // clang-format on

    RiskClass rc = RiskClass::Commodity;
    MarginType mt = MarginType::Delta;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 11182481.240302593;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify COM Delta Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                   << ", expected " << expected << ", difference "
                                                                   << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testIrVegaMargin(const boost::shared_ptr<SimmConfiguration>& config, const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM IR Vega (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::IRVol;
    ProductClass pc = ProductClass::RatesFX;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "JPY", "", "10y", "", "USD", -0.674945464, -0.674945464 });
    cl.add({ "trade_02", "", "pf", pc, rt, "JPY", "", "15y", "", "USD", 0.214918959, 0.214918959 });
    cl.add({ "trade_03", "", "pf", pc, rt, "JPY", "", "1m", "", "USD", 150.54, 150.54 });
    cl.add({ "trade_04", "", "pf", pc, rt, "JPY", "", "1y", "", "USD", 180.2179924, 180.2179924 });
    cl.add({ "trade_05", "", "pf", pc, rt, "JPY", "", "20y", "", "USD", -4.855517386, -4.855517386 });
    cl.add({ "trade_06", "", "pf", pc, rt, "JPY", "", "2w", "", "USD", 142.34, 142.34 });
    cl.add({ "trade_07", "", "pf", pc, rt, "JPY", "", "2y", "", "USD", -248.87265, -248.87265 });
    cl.add({ "trade_08", "", "pf", pc, rt, "JPY", "", "30y", "", "USD", 0.15, 0.15 });
    cl.add({ "trade_09", "", "pf", pc, rt, "JPY", "", "3m", "", "USD", 175.87, 175.87 });
    cl.add({ "trade_10", "", "pf", pc, rt, "JPY", "", "3y", "", "USD", -0.320327219, -0.320327219 });
    cl.add({ "trade_11", "", "pf", pc, rt, "JPY", "", "5y", "", "USD", -0.382417661, -0.382417661 });
    cl.add({ "trade_12", "", "pf", pc, rt, "JPY", "", "6m", "", "USD", 214.8661535, 214.8661535 });
    cl.add({ "trade_13", "", "pf", pc, rt, "USD", "", "10y", "", "USD", 0.052926029, 0.052926029 });
    cl.add({ "trade_14", "", "pf", pc, rt, "USD", "", "15y", "", "USD", 1.943209281, 1.943209281 });
    cl.add({ "trade_15", "", "pf", pc, rt, "USD", "", "1m", "", "USD", -551.1838664, -551.1838664 });
    cl.add({ "trade_16", "", "pf", pc, rt, "USD", "", "1y", "", "USD", 406.1091135, 406.1091135 });
    cl.add({ "trade_17", "", "pf", pc, rt, "USD", "", "20y", "", "USD", 1.177550257, 1.177550257 });
    cl.add({ "trade_18", "", "pf", pc, rt, "USD", "", "2w", "", "USD", -598.8791558, -598.8791558 });
    cl.add({ "trade_19", "", "pf", pc, rt, "USD", "", "2y", "", "USD", 0.011233741, 0.011233741 });
    cl.add({ "trade_20", "", "pf", pc, rt, "USD", "", "30y", "", "USD", 2.872250894, 2.872250894 });
    cl.add({ "trade_21", "", "pf", pc, rt, "USD", "", "3m", "", "USD", -1173.64531, -1173.64531 });
    cl.add({ "trade_22", "", "pf", pc, rt, "USD", "", "3y", "", "USD", 5.45, 5.45 });
    cl.add({ "trade_23", "", "pf", pc, rt, "USD", "", "5y", "", "USD", 2.65, 2.65 });
    cl.add({ "trade_24", "", "pf", pc, rt, "USD", "", "6m", "", "USD", -874.26, -874.26 });
    cl.add({ "trade_25", "", "pf", pc, rt, "BRL", "", "10y", "", "USD", 6.78, 6.78 });
    cl.add({ "trade_26", "", "pf", pc, rt, "BRL", "", "15y", "", "USD", 3.45, 3.45 });
    cl.add({ "trade_27", "", "pf", pc, rt, "BRL", "", "1m", "", "USD", -468.24, -468.24 });
    cl.add({ "trade_28", "", "pf", pc, rt, "BRL", "", "1y", "", "USD", 305.48, 305.48 });
    cl.add({ "trade_29", "", "pf", pc, rt, "BRL", "", "20y", "", "USD", 2.13, 2.13 });
    cl.add({ "trade_30", "", "pf", pc, rt, "BRL", "", "2w", "", "USD", -689.56, -689.56 });
    cl.add({ "trade_31", "", "pf", pc, rt, "BRL", "", "2y", "", "USD", 2.1, 2.1 });
    cl.add({ "trade_32", "", "pf", pc, rt, "BRL", "", "30y", "", "USD", 1.2, 1.2 });
    cl.add({ "trade_33", "", "pf", pc, rt, "BRL", "", "3m", "", "USD", -1059.63, -1059.63 });
    cl.add({ "trade_34", "", "pf", pc, rt, "BRL", "", "3y", "", "USD", 6.32, 6.32 });
    cl.add({ "trade_35", "", "pf", pc, rt, "BRL", "", "5y", "", "USD", 1.24, 1.24 });
    cl.add({ "trade_36", "", "pf", pc, rt, "BRL", "", "6m", "", "USD", -785.69, -785.69 });
    // clang-format on

    RiskClass rc = RiskClass::InterestRate;
    MarginType mt = MarginType::Vega;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 875.95718619;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify IR Vega Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                 << ", expected " << expected << ", difference "
                                                                 << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testFxVegaMargin(const boost::shared_ptr<SimmConfiguration>& config, const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM FX Vega (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::FXVol;
    ProductClass pc = ProductClass::RatesFX;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "JPYUSD", "", "10y", "", "USD", -35.10665764, -35.10665764 });
    cl.add({ "trade_02", "", "pf", pc, rt, "JPYUSD", "", "15y", "", "USD", -20652.952, -20652.952 });
    cl.add({ "trade_03", "", "pf", pc, rt, "JPYUSD", "", "1m", "", "USD", 38.8646689, 38.8646689 });
    cl.add({ "trade_04", "", "pf", pc, rt, "JPYUSD", "", "1y", "", "USD", 5671.746135, 5671.746135 });
    cl.add({ "trade_05", "", "pf", pc, rt, "JPYUSD", "", "20y", "", "USD", -298.1723999, -298.1723999 });
    cl.add({ "trade_06", "", "pf", pc, rt, "JPYUSD", "", "2w", "", "USD", 57767.22074, 57767.22074 });
    cl.add({ "trade_07", "", "pf", pc, rt, "JPYUSD", "", "2y", "", "USD", -6658.772122, -6658.772122 });
    cl.add({ "trade_08", "", "pf", pc, rt, "JPYUSD", "", "30y", "", "USD", -630.3792908, -630.3792908 });
    cl.add({ "trade_09", "", "pf", pc, rt, "JPYUSD", "", "3m", "", "USD", -10300.83413, -10300.83413 });
    cl.add({ "trade_10", "", "pf", pc, rt, "JPYUSD", "", "3y", "", "USD", 271.264799, 271.264799 });
    cl.add({ "trade_11", "", "pf", pc, rt, "JPYUSD", "", "5y", "", "USD", 45623.97027, 45623.97027 });
    cl.add({ "trade_12", "", "pf", pc, rt, "JPYUSD", "", "6m", "", "USD", 604.5913731, 604.5913731 });
    cl.add({ "trade_13", "", "pf", pc, rt, "CNYUSD", "", "10y", "", "USD", 37953.09368, 37953.09368 });
    cl.add({ "trade_14", "", "pf", pc, rt, "CNYUSD", "", "15y", "", "USD", -4131.519347, -4131.519347 });
    cl.add({ "trade_15", "", "pf", pc, rt, "CNYUSD", "", "1m", "", "USD", -95691.10948, -95691.10948 });
    cl.add({ "trade_16", "", "pf", pc, rt, "CNYUSD", "", "1y", "", "USD", -37.10975282, -37.10975282 });
    cl.add({ "trade_17", "", "pf", pc, rt, "CNYUSD", "", "20y", "", "USD", -16506.51089, -16506.51089 });
    cl.add({ "trade_18", "", "pf", pc, rt, "CNYUSD", "", "2w", "", "USD", -31.69589066, -31.69589066 });
    cl.add({ "trade_19", "", "pf", pc, rt, "CNYUSD", "", "2y", "", "USD", -20879.91655, -20879.91655 });
    cl.add({ "trade_20", "", "pf", pc, rt, "CNYUSD", "", "30y", "", "USD", -1810.415531, -1810.415531 });
    cl.add({ "trade_21", "", "pf", pc, rt, "CNYUSD", "", "3m", "", "USD", -2724.997709, -2724.997709 });
    cl.add({ "trade_22", "", "pf", pc, rt, "CNYUSD", "", "3y", "", "USD", -883.4638429, -883.4638429 });
    cl.add({ "trade_23", "", "pf", pc, rt, "CNYUSD", "", "5y", "", "USD", -4514.160233, -4514.160233 });
    cl.add({ "trade_24", "", "pf", pc, rt, "CNYUSD", "", "6m", "", "USD", 31110.56373, 31110.56373 });
    cl.add({ "trade_25", "", "pf", pc, rt, "ZAREUR", "", "10y", "", "USD", 16579.16686, 16579.16686 });
    cl.add({ "trade_26", "", "pf", pc, rt, "ZAREUR", "", "15y", "", "USD", 23.53258845, 23.53258845 });
    cl.add({ "trade_27", "", "pf", pc, rt, "ZAREUR", "", "1m", "", "USD", 3.012515508, 3.012515508 });
    cl.add({ "trade_28", "", "pf", pc, rt, "ZAREUR", "", "1y", "", "USD", -1580.295547, -1580.295547 });
    cl.add({ "trade_29", "", "pf", pc, rt, "ZAREUR", "", "20y", "", "USD", -2234.423412, -2234.423412 });
    cl.add({ "trade_30", "", "pf", pc, rt, "ZAREUR", "", "2w", "", "USD", 140.2029813, 140.2029813 });
    cl.add({ "trade_31", "", "pf", pc, rt, "ZAREUR", "", "2y", "", "USD", 113.6585936, 113.6585936 });
    cl.add({ "trade_32", "", "pf", pc, rt, "ZAREUR", "", "30y", "", "USD", -4940.603894, -4940.603894 });
    cl.add({ "trade_33", "", "pf", pc, rt, "ZAREUR", "", "3m", "", "USD", -4982.989032, -4982.989032 });
    cl.add({ "trade_34", "", "pf", pc, rt, "ZAREUR", "", "3y", "", "USD", 51131.50955, 51131.50955 });
    cl.add({ "trade_35", "", "pf", pc, rt, "ZAREUR", "", "5y", "", "USD", 115070.7572, 115070.7572 });
    cl.add({ "trade_36", "", "pf", pc, rt, "ZAREUR", "", "6m", "", "USD", 9883.176838, 9883.176838 });
    // clang-format on

    RiskClass rc = RiskClass::FX;
    MarginType mt = MarginType::Vega;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 695965.622680;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify FX Vega Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                 << ", expected " << expected << ", difference "
                                                                 << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testCrqVegaMargin(const boost::shared_ptr<SimmConfiguration>& config, const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM CRQ Vega (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::CreditVol;
    ProductClass pc = ProductClass::Credit;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "Issuer 1", "1", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_02", "", "pf", pc, rt, "Issuer 1", "1", "1y", "", "USD", 167.65, 167.65 });
    cl.add({ "trade_03", "", "pf", pc, rt, "Issuer 1", "1", "2y", "", "USD", 56.26, 56.26 });
    cl.add({ "trade_04", "", "pf", pc, rt, "Issuer 1", "1", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_05", "", "pf", pc, rt, "Issuer 1", "1", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_06", "", "pf", pc, rt, "Issuer 2", "2", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_07", "", "pf", pc, rt, "Issuer 2", "2", "1y", "", "USD", 87.15, 87.15 });
    cl.add({ "trade_08", "", "pf", pc, rt, "Issuer 2", "2", "2y", "", "USD", 6.98, 6.98 });
    cl.add({ "trade_09", "", "pf", pc, rt, "Issuer 2", "2", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_10", "", "pf", pc, rt, "Issuer 2", "2", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_11", "", "pf", pc, rt, "Issuer 3", "3", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_12", "", "pf", pc, rt, "Issuer 3", "3", "1y", "", "USD", 987.15, 987.15 });
    cl.add({ "trade_13", "", "pf", pc, rt, "Issuer 3", "3", "2y", "", "USD", 25.87, 25.87 });
    cl.add({ "trade_14", "", "pf", pc, rt, "Issuer 3", "3", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_15", "", "pf", pc, rt, "Issuer 3", "3", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_16", "", "pf", pc, rt, "Issuer 4", "4", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_17", "", "pf", pc, rt, "Issuer 4", "4", "1y", "", "USD", -65.25, -65.25 });
    cl.add({ "trade_18", "", "pf", pc, rt, "Issuer 4", "4", "2y", "", "USD", -21.12, -21.12 });
    cl.add({ "trade_19", "", "pf", pc, rt, "Issuer 4", "4", "3y", "", "USD", -45.27, -45.27 });
    cl.add({ "trade_20", "", "pf", pc, rt, "Issuer 4", "4", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_21", "", "pf", pc, rt, "Issuer 5", "5", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_22", "", "pf", pc, rt, "Issuer 5", "5", "1y", "", "USD", 457.23, 457.23 });
    cl.add({ "trade_23", "", "pf", pc, rt, "Issuer 5", "5", "2y", "", "USD", 983.27, 983.27 });
    cl.add({ "trade_24", "", "pf", pc, rt, "Issuer 5", "5", "3y", "", "USD", 2376.37, 2376.37 });
    cl.add({ "trade_25", "", "pf", pc, rt, "Issuer 5", "5", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_26", "", "pf", pc, rt, "Issuer 6", "6", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_27", "", "pf", pc, rt, "Issuer 6", "6", "1y", "", "USD", 987.26, 987.26 });
    cl.add({ "trade_28", "", "pf", pc, rt, "Issuer 6", "6", "2y", "", "USD", 23.67, 23.67 });
    cl.add({ "trade_29", "", "pf", pc, rt, "Issuer 6", "6", "3y", "", "USD", 673.21, 673.21 });
    cl.add({ "trade_30", "", "pf", pc, rt, "Issuer 6", "6", "5y", "", "USD", 985.36, 985.36 });
    cl.add({ "trade_31", "", "pf", pc, rt, "Issuer 7", "7", "10y", "", "USD", 54.32, 54.32 });
    cl.add({ "trade_32", "", "pf", pc, rt, "Issuer 7", "7", "1y", "", "USD", -3.4, -3.4 });
    cl.add({ "trade_33", "", "pf", pc, rt, "Issuer 7", "7", "2y", "", "USD", -87.43, -87.43 });
    cl.add({ "trade_34", "", "pf", pc, rt, "Issuer 7", "7", "3y", "", "USD", -74.23, -74.23 });
    cl.add({ "trade_35", "", "pf", pc, rt, "Issuer 7", "7", "5y", "", "USD", 846.32, 846.32 });
    cl.add({ "trade_36", "", "pf", pc, rt, "Issuer 8", "8", "10y", "", "USD", 203.43, 203.43 });
    cl.add({ "trade_37", "", "pf", pc, rt, "Issuer 8", "8", "1y", "", "USD", -1.34, -1.34 });
    cl.add({ "trade_38", "", "pf", pc, rt, "Issuer 8", "8", "2y", "", "USD", -43.54, -43.54 });
    cl.add({ "trade_39", "", "pf", pc, rt, "Issuer 8", "8", "3y", "", "USD", -76.43, -76.43 });
    cl.add({ "trade_40", "", "pf", pc, rt, "Issuer 8", "8", "5y", "", "USD", 765.43, 765.43 });
    cl.add({ "trade_41", "", "pf", pc, rt, "Issuer 9", "9", "10y", "", "USD", 674.32, 674.32 });
    cl.add({ "trade_42", "", "pf", pc, rt, "Issuer 9", "9", "1y", "", "USD", 32, 32 });
    cl.add({ "trade_43", "", "pf", pc, rt, "Issuer 9", "9", "2y", "", "USD", 43.21, 43.21 });
    cl.add({ "trade_44", "", "pf", pc, rt, "Issuer 9", "9", "3y", "", "USD", 9.32, 9.32 });
    cl.add({ "trade_45", "", "pf", pc, rt, "Issuer 9", "9", "5y", "", "USD", -876.65, -876.65 });
    cl.add({ "trade_46", "", "pf", pc, rt, "Issuer 10", "10", "10y", "", "USD", 122.1352924, 122.1352924 });
    cl.add({ "trade_47", "", "pf", pc, rt, "Issuer 10", "10", "1y", "", "USD", 3.21, 3.21 });
    cl.add({ "trade_48", "", "pf", pc, rt, "Issuer 10", "10", "2y", "", "USD", 4.32, 4.32 });
    cl.add({ "trade_49", "", "pf", pc, rt, "Issuer 10", "10", "3y", "", "USD", 0.021, 0.021 });
    cl.add({ "trade_50", "", "pf", pc, rt, "Issuer 10", "10", "5y", "", "USD", -56.36, -56.36 });
    cl.add({ "trade_51", "", "pf", pc, rt, "Issuer 11", "11", "10y", "", "USD", 128.7909159, 128.7909159 });
    cl.add({ "trade_52", "", "pf", pc, rt, "Issuer 11", "11", "1y", "", "USD", 0, 0 });
    cl.add({ "trade_53", "", "pf", pc, rt, "Issuer 11", "11", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_54", "", "pf", pc, rt, "Issuer 11", "11", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_55", "", "pf", pc, rt, "Issuer 11", "11", "5y", "", "USD", 65.32, 65.32 });
    cl.add({ "trade_56", "", "pf", pc, rt, "Issuer 12", "12", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_57", "", "pf", pc, rt, "Issuer 12", "12", "1y", "", "USD", 0, 0 });
    cl.add({ "trade_58", "", "pf", pc, rt, "Issuer 12", "12", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_59", "", "pf", pc, rt, "Issuer 12", "12", "3y", "", "USD", -543.76, -543.76 });
    cl.add({ "trade_60", "", "pf", pc, rt, "Issuer 12", "12", "5y", "", "USD", -73.27, -73.27 });
    cl.add({ "trade_61", "", "pf", pc, rt, "Issuer 13", "Residual", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_62", "", "pf", pc, rt, "Issuer 13", "Residual", "1y", "", "USD", -222.2007534, -222.2007534 });
    cl.add({ "trade_63", "", "pf", pc, rt, "Issuer 13", "Residual", "2y", "", "USD", -5123.406917, -5123.406917 });
    cl.add({ "trade_64", "", "pf", pc, rt, "Issuer 13", "Residual", "3y", "", "USD", -735.4936026, -735.4936026 });
    cl.add({ "trade_65", "", "pf", pc, rt, "Issuer 13", "Residual", "5y", "", "USD", 590.0888596, 590.0888596 });
    // clang-format on

    RiskClass rc = RiskClass::CreditQualifying;
    MarginType mt = MarginType::Vega;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 4311.440136002;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify CRQ Vega Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                  << ", expected " << expected << ", difference "
                                                                  << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testCrnqVegaMargin(const boost::shared_ptr<SimmConfiguration>& config, const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM CRNQ Vega (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::CreditVolNonQ;
    ProductClass pc = ProductClass::Credit;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "Issuer 1", "1", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_02", "", "pf", pc, rt, "Issuer 1", "1", "1y", "", "USD", 5673.21, 5673.21 });
    cl.add({ "trade_03", "", "pf", pc, rt, "Issuer 1", "1", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_04", "", "pf", pc, rt, "Issuer 1", "1", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_05", "", "pf", pc, rt, "Issuer 1", "1", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_06", "", "pf", pc, rt, "Issuer 2", "Residual", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_07", "", "pf", pc, rt, "Issuer 2", "Residual", "1y", "", "USD", -7432.85, -7432.85 });
    cl.add({ "trade_08", "", "pf", pc, rt, "Issuer 2", "Residual", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_09", "", "pf", pc, rt, "Issuer 2", "Residual", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_10", "", "pf", pc, rt, "Issuer 2", "Residual", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_11", "", "pf", pc, rt, "Issuer 3", "1", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_12", "", "pf", pc, rt, "Issuer 3", "1", "1y", "", "USD", 0, 0 });
    cl.add({ "trade_13", "", "pf", pc, rt, "Issuer 3", "1", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_14", "", "pf", pc, rt, "Issuer 3", "1", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_15", "", "pf", pc, rt, "Issuer 3", "1", "5y", "", "USD", 673.87, 673.87 });
    cl.add({ "trade_16", "", "pf", pc, rt, "Issuer 4", "Residual", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_17", "", "pf", pc, rt, "Issuer 4", "Residual", "1y", "", "USD", 0, 0 });
    cl.add({ "trade_18", "", "pf", pc, rt, "Issuer 4", "Residual", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_19", "", "pf", pc, rt, "Issuer 4", "Residual", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_20", "", "pf", pc, rt, "Issuer 4", "Residual", "5y", "", "USD", 982.45, 982.45 });
    cl.add({ "trade_21", "", "pf", pc, rt, "Issuer 5", "2", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_22", "", "pf", pc, rt, "Issuer 5", "2", "1y", "", "USD", -873.21, -873.21 });
    cl.add({ "trade_23", "", "pf", pc, rt, "Issuer 5", "2", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_24", "", "pf", pc, rt, "Issuer 5", "2", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_25", "", "pf", pc, rt, "Issuer 5", "2", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_26", "", "pf", pc, rt, "Issuer 6", "2", "10y", "", "USD", -673.11, -673.11 });
    cl.add({ "trade_27", "", "pf", pc, rt, "Issuer 6", "2", "1y", "", "USD", 0, 0 });
    cl.add({ "trade_28", "", "pf", pc, rt, "Issuer 6", "2", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_29", "", "pf", pc, rt, "Issuer 6", "2", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_30", "", "pf", pc, rt, "Issuer 6", "2", "5y", "", "USD", 0, 0 });
    // clang-format on

    RiskClass rc = RiskClass::CreditNonQualifying;
    MarginType mt = MarginType::Vega;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 4518.373425957;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify CRNQ Vega Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                   << ", expected " << expected << ", difference "
                                                                   << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testEqVegaMargin(const boost::shared_ptr<SimmConfiguration>& config, const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM EQ Vega (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::EquityVol;
    ProductClass pc = ProductClass::Equity;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "Index 1", "1", "10y", "", "USD", 30978, 30978 });
    cl.add({ "trade_02", "", "pf", pc, rt, "Index 2", "2", "15y", "", "USD", -84500, -84500 });
    cl.add({ "trade_03", "", "pf", pc, rt, "Index 3", "3", "1m", "", "USD", 76151, 76151 });
    cl.add({ "trade_04", "", "pf", pc, rt, "Index 4", "4", "1y", "", "USD", 33874, 33874 });
    cl.add({ "trade_05", "", "pf", pc, rt, "Index 5", "5", "20y", "", "USD", -30601, -30601 });
    cl.add({ "trade_06", "", "pf", pc, rt, "Index 6", "6", "2w", "", "USD", -7477, -7477 });
    cl.add({ "trade_07", "", "pf", pc, rt, "Index 7", "7", "2y", "", "USD", 25620, 25620 });
    cl.add({ "trade_08", "", "pf", pc, rt, "Index 8", "8", "30y", "", "USD", -93715, -93715 });
    cl.add({ "trade_09", "", "pf", pc, rt, "Index 9", "9", "3m", "", "USD", 71886, 71886 });
    cl.add({ "trade_10", "", "pf", pc, rt, "Index 10", "10", "3y", "", "USD", 89441, 89441 });
    cl.add({ "trade_11", "", "pf", pc, rt, "Index 11", "11", "5y", "", "USD", 91291, 91291 });
    cl.add({ "trade_12", "", "pf", pc, rt, "Index 12", "Residual", "6m", "", "USD", -97488, -97488 });
    cl.add({ "trade_13", "", "pf", pc, rt, "Index 13", "1", "3y", "", "USD", -83834, -83834 });
    cl.add({ "trade_14", "", "pf", pc, rt, "Index 14", "2", "6m", "", "USD", -11187, -11187 });
    cl.add({ "trade_15", "", "pf", pc, rt, "Index 15", "3", "20y", "", "USD", 72452, 72452 });
    cl.add({ "trade_16", "", "pf", pc, rt, "Index 16", "4", "15y", "", "USD", 30107, 30107 });
    cl.add({ "trade_17", "", "pf", pc, rt, "Index 17", "5", "3m", "", "USD", -63652, -63652 });
    cl.add({ "trade_18", "", "pf", pc, rt, "Index 18", "6", "10y", "", "USD", 48292, 48292 });
    cl.add({ "trade_19", "", "pf", pc, rt, "Index 19", "7", "5y", "", "USD", 47965, 47965 });
    cl.add({ "trade_20", "", "pf", pc, rt, "Index 20", "8", "1m", "", "USD", 1176, 1176 });
    cl.add({ "trade_21", "", "pf", pc, rt, "Index 21", "9", "2w", "", "USD", -77590, -77590 });
    cl.add({ "trade_22", "", "pf", pc, rt, "Index 22", "10", "1y", "", "USD", 54767, 54767 });
    cl.add({ "trade_23", "", "pf", pc, rt, "Index 23", "11", "30y", "", "USD", 27328, 27328 });
    cl.add({ "trade_24", "", "pf", pc, rt, "Index 24", "Residual", "2y", "", "USD", 11619, 11619 });
    // clang-format on

    RiskClass rc = RiskClass::Equity;
    MarginType mt = MarginType::Vega;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 4389093.666018;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify EQ Vega Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                 << ", expected " << expected << ", difference "
                                                                 << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testComVegaMargin(const boost::shared_ptr<SimmConfiguration>& config, const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM COM Vega (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::CommodityVol;
    ProductClass pc = ProductClass::Commodity;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "Coal Americas", "1", "10y", "", "USD", -1812, -1812 });
    cl.add({ "trade_02", "", "pf", pc, rt, "Crude oil Americas", "2", "15y", "", "USD", 351, 351 });
    cl.add({ "trade_03", "", "pf", pc, rt, "Light Ends Americas", "3", "1m", "", "USD", -1931, -1931 });
    cl.add({ "trade_04", "", "pf", pc, rt, "Middle Distillates Americas", "4", "1y", "", "USD", -4655, -4655 });
    cl.add({ "trade_05", "", "pf", pc, rt, "Heavy Distillates Americas", "5", "20y", "", "USD", 203, 203 });
    cl.add({ "trade_06", "", "pf", pc, rt, "NA Natural Gas Gulf Coast", "6", "2w", "", "USD", 4017, 4017 });
    cl.add({ "trade_07", "", "pf", pc, rt, "EU Natural Gas Europe", "7", "2y", "", "USD", 3534, 3534 });
    cl.add({ "trade_08", "", "pf", pc, rt, "NA Power Eastern Interconnect", "8", "30y", "", "USD", -992, -992 });
    cl.add({ "trade_09", "", "pf", pc, rt, "EU Power Germany", "9", "3m", "", "USD", -4417, -4417 });
    cl.add({ "trade_10", "", "pf", pc, rt, "Freight Wet", "10", "3y", "", "USD", -4533, -4533 });
    cl.add({ "trade_11", "", "pf", pc, rt, "Base Metals Aluminium", "11", "5y", "", "USD", 2627, 2627 });
    cl.add({ "trade_12", "", "pf", pc, rt, "Precious Metals Gold", "12", "6m", "", "USD", 1387, 1387 });
    cl.add({ "trade_13", "", "pf", pc, rt, "Grains Corn", "13", "3y", "", "USD", 488, 488 });
    cl.add({ "trade_14", "", "pf", pc, rt, "Softs Cocoa", "14", "6m", "", "USD", -17, -17 });
    cl.add({ "trade_15", "", "pf", pc, rt, "Livestock Live Cattle", "15", "20y", "", "USD", -4169, -4169 });
    cl.add({ "trade_16", "", "pf", pc, rt, "Other", "16", "15y", "", "USD", 1138, 1138 });
    cl.add({ "trade_17", "", "pf", pc, rt, "Coal Europe", "1", "3m", "", "USD", -2338, -2338 });
    cl.add({ "trade_18", "", "pf", pc, rt, "Crude oil Europe", "2", "10y", "", "USD", 882, 882 });
    cl.add({ "trade_19", "", "pf", pc, rt, "Light Ends Europe", "3", "5y", "", "USD", -153, -153 });
    cl.add({ "trade_20", "", "pf", pc, rt, "Middle Distillates Europe", "4", "1m", "", "USD", -2547, -2547 });
    cl.add({ "trade_21", "", "pf", pc, rt, "Heavy Distillates Europe", "5", "2w", "", "USD", 4248, 4248 });
    cl.add({ "trade_22", "", "pf", pc, rt, "NA Natural Gas North East", "6", "1y", "", "USD", 530, 530 });
    cl.add({ "trade_23", "", "pf", pc, rt, "EU Natural Gas Europe", "7", "30y", "", "USD", 3393, 3393 });
    cl.add({ "trade_24", "", "pf", pc, rt, "NA Power ERCOT", "8", "2y", "", "USD", -1578, -1578 });
    cl.add({ "trade_25", "", "pf", pc, rt, "EU Power UK", "9", "10y", "", "USD", 1747, 1747 });
    cl.add({ "trade_26", "", "pf", pc, rt, "Freight Dry", "10", "15y", "", "USD", 3566, 3566 });
    cl.add({ "trade_27", "", "pf", pc, rt, "Base Metals Copper", "11", "1m", "", "USD", -2706, -2706 });
    cl.add({ "trade_28", "", "pf", pc, rt, "Precious Metals Silver", "12", "1y", "", "USD", 1467, 1467 });
    cl.add({ "trade_29", "", "pf", pc, rt, "Grains Soybeans", "13", "20y", "", "USD", -4360, -4360 });
    cl.add({ "trade_30", "", "pf", pc, rt, "Softs Coffee", "14", "2w", "", "USD", 4105, 4105 });
    cl.add({ "trade_31", "", "pf", pc, rt, "Livestock Feeder Cattle", "15", "2y", "", "USD", -1046, -1046 });
    cl.add({ "trade_32", "", "pf", pc, rt, "Other", "16", "30y", "", "USD", 4295, 4295 });
    // clang-format on

    RiskClass rc = RiskClass::Commodity;
    MarginType mt = MarginType::Vega;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 343281.522636407;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify COM Vega Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                  << ", expected " << expected << ", difference "
                                                                  << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testIrCurvatureMargin(const boost::shared_ptr<SimmConfiguration>& config,
                           const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM IR Curvature (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::IRVol;
    ProductClass pc = ProductClass::RatesFX;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "JPY", "", "10y", "", "USD", -0.674945464, -0.674945464 });
    cl.add({ "trade_02", "", "pf", pc, rt, "JPY", "", "15y", "", "USD", 0.214918959, 0.214918959 });
    cl.add({ "trade_03", "", "pf", pc, rt, "JPY", "", "1m", "", "USD", 150.54, 150.54 });
    cl.add({ "trade_04", "", "pf", pc, rt, "JPY", "", "1y", "", "USD", 180.2179924, 180.2179924 });
    cl.add({ "trade_05", "", "pf", pc, rt, "JPY", "", "20y", "", "USD", -4.855517386, -4.855517386 });
    cl.add({ "trade_06", "", "pf", pc, rt, "JPY", "", "2w", "", "USD", 142.34, 142.34 });
    cl.add({ "trade_07", "", "pf", pc, rt, "JPY", "", "2y", "", "USD", -248.87265, -248.87265 });
    cl.add({ "trade_08", "", "pf", pc, rt, "JPY", "", "30y", "", "USD", 0.15, 0.15 });
    cl.add({ "trade_09", "", "pf", pc, rt, "JPY", "", "3m", "", "USD", 175.87, 175.87 });
    cl.add({ "trade_10", "", "pf", pc, rt, "JPY", "", "3y", "", "USD", -0.320327219, -0.320327219 });
    cl.add({ "trade_11", "", "pf", pc, rt, "JPY", "", "5y", "", "USD", -0.382417661, -0.382417661 });
    cl.add({ "trade_12", "", "pf", pc, rt, "JPY", "", "6m", "", "USD", 214.8661535, 214.8661535 });
    cl.add({ "trade_13", "", "pf", pc, rt, "USD", "", "10y", "", "USD", 0.052926029, 0.052926029 });
    cl.add({ "trade_14", "", "pf", pc, rt, "USD", "", "15y", "", "USD", 1.943209281, 1.943209281 });
    cl.add({ "trade_15", "", "pf", pc, rt, "USD", "", "1m", "", "USD", -551.1838664, -551.1838664 });
    cl.add({ "trade_16", "", "pf", pc, rt, "USD", "", "1y", "", "USD", 406.1091135, 406.1091135 });
    cl.add({ "trade_17", "", "pf", pc, rt, "USD", "", "20y", "", "USD", 1.177550257, 1.177550257 });
    cl.add({ "trade_18", "", "pf", pc, rt, "USD", "", "2w", "", "USD", -598.8791558, -598.8791558 });
    cl.add({ "trade_19", "", "pf", pc, rt, "USD", "", "2y", "", "USD", 0.011233741, 0.011233741 });
    cl.add({ "trade_20", "", "pf", pc, rt, "USD", "", "30y", "", "USD", 2.872250894, 2.872250894 });
    cl.add({ "trade_21", "", "pf", pc, rt, "USD", "", "3m", "", "USD", -1173.64531, -1173.64531 });
    cl.add({ "trade_22", "", "pf", pc, rt, "USD", "", "3y", "", "USD", 5.45, 5.45 });
    cl.add({ "trade_23", "", "pf", pc, rt, "USD", "", "5y", "", "USD", 2.65, 2.65 });
    cl.add({ "trade_24", "", "pf", pc, rt, "USD", "", "6m", "", "USD", -874.26, -874.26 });
    cl.add({ "trade_25", "", "pf", pc, rt, "BRL", "", "10y", "", "USD", 6.78, 6.78 });
    cl.add({ "trade_26", "", "pf", pc, rt, "BRL", "", "15y", "", "USD", 3.45, 3.45 });
    cl.add({ "trade_27", "", "pf", pc, rt, "BRL", "", "1m", "", "USD", -468.24, -468.24 });
    cl.add({ "trade_28", "", "pf", pc, rt, "BRL", "", "1y", "", "USD", 305.48, 305.48 });
    cl.add({ "trade_29", "", "pf", pc, rt, "BRL", "", "20y", "", "USD", 2.13, 2.13 });
    cl.add({ "trade_30", "", "pf", pc, rt, "BRL", "", "2w", "", "USD", -689.56, -689.56 });
    cl.add({ "trade_31", "", "pf", pc, rt, "BRL", "", "2y", "", "USD", 2.1, 2.1 });
    cl.add({ "trade_32", "", "pf", pc, rt, "BRL", "", "30y", "", "USD", 1.2, 1.2 });
    cl.add({ "trade_33", "", "pf", pc, rt, "BRL", "", "3m", "", "USD", -1059.63, -1059.63 });
    cl.add({ "trade_34", "", "pf", pc, rt, "BRL", "", "3y", "", "USD", 6.32, 6.32 });
    cl.add({ "trade_35", "", "pf", pc, rt, "BRL", "", "5y", "", "USD", 1.24, 1.24 });
    cl.add({ "trade_36", "", "pf", pc, rt, "BRL", "", "6m", "", "USD", -785.69, -785.69 });
    // clang-format on

    RiskClass rc = RiskClass::InterestRate;
    MarginType mt = MarginType::Curvature;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 1525.938767;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify IR Curvature Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                      << ", expected " << expected << ", difference "
                                                                      << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testFxCurvatureMargin(const boost::shared_ptr<SimmConfiguration>& config,
                           const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM FX Curvature (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::FXVol;
    ProductClass pc = ProductClass::RatesFX;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "JPYUSD", "", "10y", "", "USD", -35.10665764, -35.10665764 });
    cl.add({ "trade_02", "", "pf", pc, rt, "JPYUSD", "", "15y", "", "USD", -20652.952, -20652.952 });
    cl.add({ "trade_03", "", "pf", pc, rt, "JPYUSD", "", "1m", "", "USD", 38.8646689, 38.8646689 });
    cl.add({ "trade_04", "", "pf", pc, rt, "JPYUSD", "", "1y", "", "USD", 5671.746135, 5671.746135 });
    cl.add({ "trade_05", "", "pf", pc, rt, "JPYUSD", "", "20y", "", "USD", -298.1723999, -298.1723999 });
    cl.add({ "trade_06", "", "pf", pc, rt, "JPYUSD", "", "2w", "", "USD", 57767.22074, 57767.22074 });
    cl.add({ "trade_07", "", "pf", pc, rt, "JPYUSD", "", "2y", "", "USD", -6658.772122, -6658.772122 });
    cl.add({ "trade_08", "", "pf", pc, rt, "JPYUSD", "", "30y", "", "USD", -630.3792908, -630.3792908 });
    cl.add({ "trade_09", "", "pf", pc, rt, "JPYUSD", "", "3m", "", "USD", -10300.83413, -10300.83413 });
    cl.add({ "trade_10", "", "pf", pc, rt, "JPYUSD", "", "3y", "", "USD", 271.264799, 271.264799 });
    cl.add({ "trade_11", "", "pf", pc, rt, "JPYUSD", "", "5y", "", "USD", 45623.97027, 45623.97027 });
    cl.add({ "trade_12", "", "pf", pc, rt, "JPYUSD", "", "6m", "", "USD", 604.5913731, 604.5913731 });
    cl.add({ "trade_13", "", "pf", pc, rt, "CNYUSD", "", "10y", "", "USD", 37953.09368, 37953.09368 });
    cl.add({ "trade_14", "", "pf", pc, rt, "CNYUSD", "", "15y", "", "USD", -4131.519347, -4131.519347 });
    cl.add({ "trade_15", "", "pf", pc, rt, "CNYUSD", "", "1m", "", "USD", -95691.10948, -95691.10948 });
    cl.add({ "trade_16", "", "pf", pc, rt, "CNYUSD", "", "1y", "", "USD", -37.10975282, -37.10975282 });
    cl.add({ "trade_17", "", "pf", pc, rt, "CNYUSD", "", "20y", "", "USD", -16506.51089, -16506.51089 });
    cl.add({ "trade_18", "", "pf", pc, rt, "CNYUSD", "", "2w", "", "USD", -31.69589066, -31.69589066 });
    cl.add({ "trade_19", "", "pf", pc, rt, "CNYUSD", "", "2y", "", "USD", -20879.91655, -20879.91655 });
    cl.add({ "trade_20", "", "pf", pc, rt, "CNYUSD", "", "30y", "", "USD", -1810.415531, -1810.415531 });
    cl.add({ "trade_21", "", "pf", pc, rt, "CNYUSD", "", "3m", "", "USD", -2724.997709, -2724.997709 });
    cl.add({ "trade_22", "", "pf", pc, rt, "CNYUSD", "", "3y", "", "USD", -883.4638429, -883.4638429 });
    cl.add({ "trade_23", "", "pf", pc, rt, "CNYUSD", "", "5y", "", "USD", -4514.160233, -4514.160233 });
    cl.add({ "trade_24", "", "pf", pc, rt, "CNYUSD", "", "6m", "", "USD", 31110.56373, 31110.56373 });
    cl.add({ "trade_25", "", "pf", pc, rt, "ZAREUR", "", "10y", "", "USD", 16579.16686, 16579.16686 });
    cl.add({ "trade_26", "", "pf", pc, rt, "ZAREUR", "", "15y", "", "USD", 23.53258845, 23.53258845 });
    cl.add({ "trade_27", "", "pf", pc, rt, "ZAREUR", "", "1m", "", "USD", 3.012515508, 3.012515508 });
    cl.add({ "trade_28", "", "pf", pc, rt, "ZAREUR", "", "1y", "", "USD", -1580.295547, -1580.295547 });
    cl.add({ "trade_29", "", "pf", pc, rt, "ZAREUR", "", "20y", "", "USD", -2234.423412, -2234.423412 });
    cl.add({ "trade_30", "", "pf", pc, rt, "ZAREUR", "", "2w", "", "USD", 140.2029813, 140.2029813 });
    cl.add({ "trade_31", "", "pf", pc, rt, "ZAREUR", "", "2y", "", "USD", 113.6585936, 113.6585936 });
    cl.add({ "trade_32", "", "pf", pc, rt, "ZAREUR", "", "30y", "", "USD", -4940.603894, -4940.603894 });
    cl.add({ "trade_33", "", "pf", pc, rt, "ZAREUR", "", "3m", "", "USD", -4982.989032, -4982.989032 });
    cl.add({ "trade_34", "", "pf", pc, rt, "ZAREUR", "", "3y", "", "USD", 51131.50955, 51131.50955 });
    cl.add({ "trade_35", "", "pf", pc, rt, "ZAREUR", "", "5y", "", "USD", 115070.7572, 115070.7572 });
    cl.add({ "trade_36", "", "pf", pc, rt, "ZAREUR", "", "6m", "", "USD", 9883.176838, 9883.176838 });
    // clang-format on

    RiskClass rc = RiskClass::FX;
    MarginType mt = MarginType::Curvature;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 3157930.974429;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify FX Curvature Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                      << ", expected " << expected << ", difference "
                                                                      << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testCrqCurvatureMargin(const boost::shared_ptr<SimmConfiguration>& config,
                            const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM CRQ Curvature (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::CreditVol;
    ProductClass pc = ProductClass::Credit;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "Issuer 1", "1", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_02", "", "pf", pc, rt, "Issuer 1", "1", "1y", "", "USD", 167.65, 167.65 });
    cl.add({ "trade_03", "", "pf", pc, rt, "Issuer 1", "1", "2y", "", "USD", 56.26, 56.26 });
    cl.add({ "trade_04", "", "pf", pc, rt, "Issuer 1", "1", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_05", "", "pf", pc, rt, "Issuer 1", "1", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_06", "", "pf", pc, rt, "Issuer 2", "2", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_07", "", "pf", pc, rt, "Issuer 2", "2", "1y", "", "USD", 87.15, 87.15 });
    cl.add({ "trade_08", "", "pf", pc, rt, "Issuer 2", "2", "2y", "", "USD", 6.98, 6.98 });
    cl.add({ "trade_09", "", "pf", pc, rt, "Issuer 2", "2", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_10", "", "pf", pc, rt, "Issuer 2", "2", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_11", "", "pf", pc, rt, "Issuer 3", "3", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_12", "", "pf", pc, rt, "Issuer 3", "3", "1y", "", "USD", 987.15, 987.15 });
    cl.add({ "trade_13", "", "pf", pc, rt, "Issuer 3", "3", "2y", "", "USD", 25.87, 25.87 });
    cl.add({ "trade_14", "", "pf", pc, rt, "Issuer 3", "3", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_15", "", "pf", pc, rt, "Issuer 3", "3", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_16", "", "pf", pc, rt, "Issuer 4", "4", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_17", "", "pf", pc, rt, "Issuer 4", "4", "1y", "", "USD", -65.25, -65.25 });
    cl.add({ "trade_18", "", "pf", pc, rt, "Issuer 4", "4", "2y", "", "USD", -21.12, -21.12 });
    cl.add({ "trade_19", "", "pf", pc, rt, "Issuer 4", "4", "3y", "", "USD", -45.27, -45.27 });
    cl.add({ "trade_20", "", "pf", pc, rt, "Issuer 4", "4", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_21", "", "pf", pc, rt, "Issuer 5", "5", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_22", "", "pf", pc, rt, "Issuer 5", "5", "1y", "", "USD", 457.23, 457.23 });
    cl.add({ "trade_23", "", "pf", pc, rt, "Issuer 5", "5", "2y", "", "USD", 983.27, 983.27 });
    cl.add({ "trade_24", "", "pf", pc, rt, "Issuer 5", "5", "3y", "", "USD", 2376.37, 2376.37 });
    cl.add({ "trade_25", "", "pf", pc, rt, "Issuer 5", "5", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_26", "", "pf", pc, rt, "Issuer 6", "6", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_27", "", "pf", pc, rt, "Issuer 6", "6", "1y", "", "USD", 987.26, 987.26 });
    cl.add({ "trade_28", "", "pf", pc, rt, "Issuer 6", "6", "2y", "", "USD", 23.67, 23.67 });
    cl.add({ "trade_29", "", "pf", pc, rt, "Issuer 6", "6", "3y", "", "USD", 673.21, 673.21 });
    cl.add({ "trade_30", "", "pf", pc, rt, "Issuer 6", "6", "5y", "", "USD", 985.36, 985.36 });
    cl.add({ "trade_31", "", "pf", pc, rt, "Issuer 7", "7", "10y", "", "USD", 54.32, 54.32 });
    cl.add({ "trade_32", "", "pf", pc, rt, "Issuer 7", "7", "1y", "", "USD", -3.4, -3.4 });
    cl.add({ "trade_33", "", "pf", pc, rt, "Issuer 7", "7", "2y", "", "USD", -87.43, -87.43 });
    cl.add({ "trade_34", "", "pf", pc, rt, "Issuer 7", "7", "3y", "", "USD", -74.23, -74.23 });
    cl.add({ "trade_35", "", "pf", pc, rt, "Issuer 7", "7", "5y", "", "USD", 846.32, 846.32 });
    cl.add({ "trade_36", "", "pf", pc, rt, "Issuer 8", "8", "10y", "", "USD", 203.43, 203.43 });
    cl.add({ "trade_37", "", "pf", pc, rt, "Issuer 8", "8", "1y", "", "USD", -1.34, -1.34 });
    cl.add({ "trade_38", "", "pf", pc, rt, "Issuer 8", "8", "2y", "", "USD", -43.54, -43.54 });
    cl.add({ "trade_39", "", "pf", pc, rt, "Issuer 8", "8", "3y", "", "USD", -76.43, -76.43 });
    cl.add({ "trade_40", "", "pf", pc, rt, "Issuer 8", "8", "5y", "", "USD", 765.43, 765.43 });
    cl.add({ "trade_41", "", "pf", pc, rt, "Issuer 9", "9", "10y", "", "USD", 674.32, 674.32 });
    cl.add({ "trade_42", "", "pf", pc, rt, "Issuer 9", "9", "1y", "", "USD", 32, 32 });
    cl.add({ "trade_43", "", "pf", pc, rt, "Issuer 9", "9", "2y", "", "USD", 43.21, 43.21 });
    cl.add({ "trade_44", "", "pf", pc, rt, "Issuer 9", "9", "3y", "", "USD", 9.32, 9.32 });
    cl.add({ "trade_45", "", "pf", pc, rt, "Issuer 9", "9", "5y", "", "USD", -876.65, -876.65 });
    cl.add({ "trade_46", "", "pf", pc, rt, "Issuer 10", "10", "10y", "", "USD", 122.1352924, 122.1352924 });
    cl.add({ "trade_47", "", "pf", pc, rt, "Issuer 10", "10", "1y", "", "USD", 3.21, 3.21 });
    cl.add({ "trade_48", "", "pf", pc, rt, "Issuer 10", "10", "2y", "", "USD", 4.32, 4.32 });
    cl.add({ "trade_49", "", "pf", pc, rt, "Issuer 10", "10", "3y", "", "USD", 0.021, 0.021 });
    cl.add({ "trade_50", "", "pf", pc, rt, "Issuer 10", "10", "5y", "", "USD", -56.36, -56.36 });
    cl.add({ "trade_51", "", "pf", pc, rt, "Issuer 11", "11", "10y", "", "USD", 128.7909159, 128.7909159 });
    cl.add({ "trade_52", "", "pf", pc, rt, "Issuer 11", "11", "1y", "", "USD", 0, 0 });
    cl.add({ "trade_53", "", "pf", pc, rt, "Issuer 11", "11", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_54", "", "pf", pc, rt, "Issuer 11", "11", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_55", "", "pf", pc, rt, "Issuer 11", "11", "5y", "", "USD", 65.32, 65.32 });
    cl.add({ "trade_56", "", "pf", pc, rt, "Issuer 12", "12", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_57", "", "pf", pc, rt, "Issuer 12", "12", "1y", "", "USD", 0, 0 });
    cl.add({ "trade_58", "", "pf", pc, rt, "Issuer 12", "12", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_59", "", "pf", pc, rt, "Issuer 12", "12", "3y", "", "USD", -543.76, -543.76 });
    cl.add({ "trade_60", "", "pf", pc, rt, "Issuer 12", "12", "5y", "", "USD", -73.27, -73.27 });
    cl.add({ "trade_61", "", "pf", pc, rt, "Issuer 13", "Residual", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_62", "", "pf", pc, rt, "Issuer 13", "Residual", "1y", "", "USD", -222.2007534, -222.2007534 });
    cl.add({ "trade_63", "", "pf", pc, rt, "Issuer 13", "Residual", "2y", "", "USD", -5123.406917, -5123.406917 });
    cl.add({ "trade_64", "", "pf", pc, rt, "Issuer 13", "Residual", "3y", "", "USD", -735.4936026, -735.4936026 });
    cl.add({ "trade_65", "", "pf", pc, rt, "Issuer 13", "Residual", "5y", "", "USD", 590.0888596, 590.0888596 });
    // clang-format on

    RiskClass rc = RiskClass::CreditQualifying;
    MarginType mt = MarginType::Curvature;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 428.019768;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify CRQ Curvature Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                       << ", expected " << expected << ", difference "
                                                                       << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testCrnqCurvatureMargin(const boost::shared_ptr<SimmConfiguration>& config,
                             const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM CRNQ Curvature (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::CreditVolNonQ;
    ProductClass pc = ProductClass::Credit;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "Issuer 1", "1", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_02", "", "pf", pc, rt, "Issuer 1", "1", "1y", "", "USD", 5673.21, 5673.21 });
    cl.add({ "trade_03", "", "pf", pc, rt, "Issuer 1", "1", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_04", "", "pf", pc, rt, "Issuer 1", "1", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_05", "", "pf", pc, rt, "Issuer 1", "1", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_06", "", "pf", pc, rt, "Issuer 2", "Residual", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_07", "", "pf", pc, rt, "Issuer 2", "Residual", "1y", "", "USD", -7432.85, -7432.85 });
    cl.add({ "trade_08", "", "pf", pc, rt, "Issuer 2", "Residual", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_09", "", "pf", pc, rt, "Issuer 2", "Residual", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_10", "", "pf", pc, rt, "Issuer 2", "Residual", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_11", "", "pf", pc, rt, "Issuer 3", "1", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_12", "", "pf", pc, rt, "Issuer 3", "1", "1y", "", "USD", 0, 0 });
    cl.add({ "trade_13", "", "pf", pc, rt, "Issuer 3", "1", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_14", "", "pf", pc, rt, "Issuer 3", "1", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_15", "", "pf", pc, rt, "Issuer 3", "1", "5y", "", "USD", 673.87, 673.87 });
    cl.add({ "trade_16", "", "pf", pc, rt, "Issuer 4", "Residual", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_17", "", "pf", pc, rt, "Issuer 4", "Residual", "1y", "", "USD", 0, 0 });
    cl.add({ "trade_18", "", "pf", pc, rt, "Issuer 4", "Residual", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_19", "", "pf", pc, rt, "Issuer 4", "Residual", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_20", "", "pf", pc, rt, "Issuer 4", "Residual", "5y", "", "USD", 982.45, 982.45 });
    cl.add({ "trade_21", "", "pf", pc, rt, "Issuer 5", "2", "10y", "", "USD", 0, 0 });
    cl.add({ "trade_22", "", "pf", pc, rt, "Issuer 5", "2", "1y", "", "USD", -873.21, -873.21 });
    cl.add({ "trade_23", "", "pf", pc, rt, "Issuer 5", "2", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_24", "", "pf", pc, rt, "Issuer 5", "2", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_25", "", "pf", pc, rt, "Issuer 5", "2", "5y", "", "USD", 0, 0 });
    cl.add({ "trade_26", "", "pf", pc, rt, "Issuer 6", "2", "10y", "", "USD", -673.11, -673.11 });
    cl.add({ "trade_27", "", "pf", pc, rt, "Issuer 6", "2", "1y", "", "USD", 0, 0 });
    cl.add({ "trade_28", "", "pf", pc, rt, "Issuer 6", "2", "2y", "", "USD", 0, 0 });
    cl.add({ "trade_29", "", "pf", pc, rt, "Issuer 6", "2", "3y", "", "USD", 0, 0 });
    cl.add({ "trade_30", "", "pf", pc, rt, "Issuer 6", "2", "5y", "", "USD", 0, 0 });
    // clang-format on

    RiskClass rc = RiskClass::CreditNonQualifying;
    MarginType mt = MarginType::Curvature;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 751.005539;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify CRNQ Curvature Margin, computed "
                    << std::setprecision(6) << std::fixed << margin << ", expected " << expected << ", difference "
                    << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testEqCurvatureMargin(const boost::shared_ptr<SimmConfiguration>& config,
                           const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM EQ Curvature (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::EquityVol;
    ProductClass pc = ProductClass::Equity;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "Index 1", "1", "10y", "", "USD", 30978, 30978 });
    cl.add({ "trade_02", "", "pf", pc, rt, "Index 2", "2", "15y", "", "USD", -84500, -84500 });
    cl.add({ "trade_03", "", "pf", pc, rt, "Index 3", "3", "1m", "", "USD", 76151, 76151 });
    cl.add({ "trade_04", "", "pf", pc, rt, "Index 4", "4", "1y", "", "USD", 33874, 33874 });
    cl.add({ "trade_05", "", "pf", pc, rt, "Index 5", "5", "20y", "", "USD", -30601, -30601 });
    cl.add({ "trade_06", "", "pf", pc, rt, "Index 6", "6", "2w", "", "USD", -7477, -7477 });
    cl.add({ "trade_07", "", "pf", pc, rt, "Index 7", "7", "2y", "", "USD", 25620, 25620 });
    cl.add({ "trade_08", "", "pf", pc, rt, "Index 8", "8", "30y", "", "USD", -93715, -93715 });
    cl.add({ "trade_09", "", "pf", pc, rt, "Index 9", "9", "3m", "", "USD", 71886, 71886 });
    cl.add({ "trade_10", "", "pf", pc, rt, "Index 10", "10", "3y", "", "USD", 89441, 89441 });
    cl.add({ "trade_11", "", "pf", pc, rt, "Index 11", "11", "5y", "", "USD", 91291, 91291 });
    cl.add({ "trade_12", "", "pf", pc, rt, "Index 12", "Residual", "6m", "", "USD", -97488, -97488 });
    cl.add({ "trade_13", "", "pf", pc, rt, "Index 13", "1", "3y", "", "USD", -83834, -83834 });
    cl.add({ "trade_14", "", "pf", pc, rt, "Index 14", "2", "6m", "", "USD", -11187, -11187 });
    cl.add({ "trade_15", "", "pf", pc, rt, "Index 15", "3", "20y", "", "USD", 72452, 72452 });
    cl.add({ "trade_16", "", "pf", pc, rt, "Index 16", "4", "15y", "", "USD", 30107, 30107 });
    cl.add({ "trade_17", "", "pf", pc, rt, "Index 17", "5", "3m", "", "USD", -63652, -63652 });
    cl.add({ "trade_18", "", "pf", pc, rt, "Index 18", "6", "10y", "", "USD", 48292, 48292 });
    cl.add({ "trade_19", "", "pf", pc, rt, "Index 19", "7", "5y", "", "USD", 47965, 47965 });
    cl.add({ "trade_20", "", "pf", pc, rt, "Index 20", "8", "1m", "", "USD", 1176, 1176 });
    cl.add({ "trade_21", "", "pf", pc, rt, "Index 21", "9", "2w", "", "USD", -77590, -77590 });
    cl.add({ "trade_22", "", "pf", pc, rt, "Index 22", "10", "1y", "", "USD", 54767, 54767 });
    cl.add({ "trade_23", "", "pf", pc, rt, "Index 23", "11", "30y", "", "USD", 27328, 27328 });
    cl.add({ "trade_24", "", "pf", pc, rt, "Index 24", "Residual", "2y", "", "USD", 11619, 11619 });
    // clang-format on

    RiskClass rc = RiskClass::Equity;
    MarginType mt = MarginType::Curvature;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 10011244.779063;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify EQ Curvature Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                      << ", expected " << expected << ", difference "
                                                                      << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testComCurvatureMargin(const boost::shared_ptr<SimmConfiguration>& config,
                            const boost::shared_ptr<Market>& market) {

    BOOST_TEST_MESSAGE("Testing SIMM COM Curvature (" << config->name() << ") ...");

    CrifLoader cl(config, true);
    RiskType rt = RiskType::CommodityVol;
    ProductClass pc = ProductClass::Commodity;

    // clang-format off
    cl.add({ "trade_01", "", "pf", pc, rt, "Coal Americas", "1", "10y", "", "USD", -1812, -1812 });
    cl.add({ "trade_02", "", "pf", pc, rt, "Crude oil Americas", "2", "15y", "", "USD", 351, 351 });
    cl.add({ "trade_03", "", "pf", pc, rt, "Light Ends Americas", "3", "1m", "", "USD", -1931, -1931 });
    cl.add({ "trade_04", "", "pf", pc, rt, "Middle Distillates Americas", "4", "1y", "", "USD", -4655, -4655 });
    cl.add({ "trade_05", "", "pf", pc, rt, "Heavy Distillates Americas", "5", "20y", "", "USD", 203, 203 });
    cl.add({ "trade_06", "", "pf", pc, rt, "NA Natural Gas Gulf Coast", "6", "2w", "", "USD", 4017, 4017 });
    cl.add({ "trade_07", "", "pf", pc, rt, "EU Natural Gas Europe", "7", "2y", "", "USD", 3534, 3534 });
    cl.add({ "trade_08", "", "pf", pc, rt, "NA Power Eastern Interconnect", "8", "30y", "", "USD", -992, -992 });
    cl.add({ "trade_09", "", "pf", pc, rt, "EU Power Germany", "9", "3m", "", "USD", -4417, -4417 });
    cl.add({ "trade_10", "", "pf", pc, rt, "Freight Wet", "10", "3y", "", "USD", -4533, -4533 });
    cl.add({ "trade_11", "", "pf", pc, rt, "Base Metals Aluminium", "11", "5y", "", "USD", 2627, 2627 });
    cl.add({ "trade_12", "", "pf", pc, rt, "Precious Metals Gold", "12", "6m", "", "USD", 1387, 1387 });
    cl.add({ "trade_13", "", "pf", pc, rt, "Grains Corn", "13", "3y", "", "USD", 488, 488 });
    cl.add({ "trade_14", "", "pf", pc, rt, "Softs Cocoa", "14", "6m", "", "USD", -17, -17 });
    cl.add({ "trade_15", "", "pf", pc, rt, "Livestock Live Cattle", "15", "20y", "", "USD", -4169, -4169 });
    cl.add({ "trade_16", "", "pf", pc, rt, "Other", "16", "15y", "", "USD", 1138, 1138 });
    cl.add({ "trade_17", "", "pf", pc, rt, "Coal Europe", "1", "3m", "", "USD", -2338, -2338 });
    cl.add({ "trade_18", "", "pf", pc, rt, "Crude oil Europe", "2", "10y", "", "USD", 882, 882 });
    cl.add({ "trade_19", "", "pf", pc, rt, "Light Ends Europe", "3", "5y", "", "USD", -153, -153 });
    cl.add({ "trade_20", "", "pf", pc, rt, "Middle Distillates Europe", "4", "1m", "", "USD", -2547, -2547 });
    cl.add({ "trade_21", "", "pf", pc, rt, "Heavy Distillates Europe", "5", "2w", "", "USD", 4248, 4248 });
    cl.add({ "trade_22", "", "pf", pc, rt, "NA Natural Gas North East", "6", "1y", "", "USD", 530, 530 });
    cl.add({ "trade_23", "", "pf", pc, rt, "EU Natural Gas Europe", "7", "30y", "", "USD", 3393, 3393 });
    cl.add({ "trade_24", "", "pf", pc, rt, "NA Power ERCOT", "8", "2y", "", "USD", -1578, -1578 });
    cl.add({ "trade_25", "", "pf", pc, rt, "EU Power UK", "9", "10y", "", "USD", 1747, 1747 });
    cl.add({ "trade_26", "", "pf", pc, rt, "Freight Dry", "10", "15y", "", "USD", 3566, 3566 });
    cl.add({ "trade_27", "", "pf", pc, rt, "Base Metals Copper", "11", "1m", "", "USD", -2706, -2706 });
    cl.add({ "trade_28", "", "pf", pc, rt, "Precious Metals Silver", "12", "1y", "", "USD", 1467, 1467 });
    cl.add({ "trade_29", "", "pf", pc, rt, "Grains Soybeans", "13", "20y", "", "USD", -4360, -4360 });
    cl.add({ "trade_30", "", "pf", pc, rt, "Softs Coffee", "14", "2w", "", "USD", 4105, 4105 });
    cl.add({ "trade_31", "", "pf", pc, rt, "Livestock Feeder Cattle", "15", "2y", "", "USD", -1046, -1046 });
    cl.add({ "trade_32", "", "pf", pc, rt, "Other", "16", "30y", "", "USD", 4295, 4295 });
    // clang-format on

    RiskClass rc = RiskClass::Commodity;
    MarginType mt = MarginType::Curvature;

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;
    Real margin = simmResults.get(pc, rc, mt, "All");

    Real tol = 1.0E-6;
    Real expected = 949078.392090607;
    if (std::abs(margin - expected) > tol)
        BOOST_ERROR("Failed to verify COM Curvature Margin, computed " << std::setprecision(6) << std::fixed << margin
                                                                       << ", expected " << expected << ", difference "
                                                                       << (margin - expected) << ", tolerance " << tol);
    BOOST_CHECK(true);
}

void testMarginAggregation(const boost::shared_ptr<SimmConfiguration>& config,
                           const boost::shared_ptr<Market>& market) {

    // Checking what the SIMM calculator gives vs. aggregating manually

    BOOST_TEST_MESSAGE("Testing SIMM Margin Aggregation (" << config->name() << ") ...");

    CrifLoader cl(config, true);

    // clang-format off
    cl.add({ "trade_01", "", "pf", ProductClass::RatesFX, RiskType::IRCurve, "USD", "1", "5y", "Libor1m", "USD", 1053.45, 1053.45 });
    cl.add({ "trade_02", "", "pf", ProductClass::Credit, RiskType::IRCurve, "USD", "1", "5y", "Libor1m", "USD", 2053.45, 2053.45 });
    cl.add({ "trade_03", "", "pf", ProductClass::Equity, RiskType::IRCurve, "USD", "1", "5y", "Libor1m", "USD", 3053.45, 3053.45 });
    cl.add({ "trade_04", "", "pf", ProductClass::Commodity, RiskType::IRCurve, "USD", "1", "5y", "Libor1m", "USD", 4053.45, 4053.45 });
    cl.add({ "trade_05", "", "pf", ProductClass::RatesFX, RiskType::FX, "IDR", "", "", "", "USD", 5402.350999, 5402.350999 });
    cl.add({ "trade_06", "", "pf", ProductClass::RatesFX, RiskType::FX, "JPY", "", "", "", "USD", -34390.56314, -34390.56314 });
    cl.add({ "trade_07", "", "pf", ProductClass::Credit, RiskType::FX, "IDR", "", "", "", "USD", 5402.350999, 5402.350999 });
    cl.add({ "trade_08", "", "pf", ProductClass::Credit, RiskType::FX, "JPY", "", "", "", "USD", -34390.56314, -34390.56314 });
    cl.add({ "trade_09", "", "pf", ProductClass::Equity, RiskType::FX, "IDR", "", "", "", "USD", 5402.350999, 5402.350999 });
    cl.add({ "trade_10", "", "pf", ProductClass::Equity, RiskType::FX, "JPY", "", "", "", "USD", -34390.56314, -34390.56314 });
    cl.add({ "trade_11", "", "pf", ProductClass::Commodity, RiskType::FX, "IDR", "", "", "", "USD", 5402.350999, 5402.350999 });
    cl.add({ "trade_12", "", "pf", ProductClass::Commodity, RiskType::FX, "JPY", "", "", "", "USD", -34390.56314, -34390.56314 });
    cl.add({ "trade_13", "", "pf", ProductClass::Credit, RiskType::CreditQ, "Issuer 1", "1", "1y", "", "USD", 8050, 8050 });
    cl.add({ "trade_14", "", "pf", ProductClass::Credit, RiskType::CreditNonQ, "Issuer 1", "1", "1y", "", "USD", -1544.867056, -1544.867056 });
    cl.add({ "trade_15", "", "pf", ProductClass::Equity, RiskType::Equity, "Index 1", "1", "", "", "USD", 1730.821481, 1730.821481 });
    cl.add({ "trade_16", "", "pf", ProductClass::Commodity, RiskType::Commodity, "Coal Americas", "1", "", "", "USD", -2335.613204, -2335.613204 });
    cl.add({ "trade_17", "", "pf", ProductClass::RatesFX, RiskType::IRVol, "JPY", "", "1y", "", "USD", 180.2179924, 180.2179924 });
    cl.add({ "trade_18", "", "pf", ProductClass::RatesFX, RiskType::FXVol, "JPYUSD", "", "15y", "", "USD", -20652.952, -20652.952 });
    cl.add({ "trade_19", "", "pf", ProductClass::Credit, RiskType::CreditVol, "Issuer 1", "1", "1y", "", "USD", 167.65, 167.65 });
    cl.add({ "trade_20", "", "pf", ProductClass::Credit, RiskType::CreditVolNonQ, "Issuer 1", "1", "1y", "", "USD", 5673.21, 5673.21 });
    cl.add({ "trade_21", "", "pf", ProductClass::Equity, RiskType::EquityVol, "Index 1", "1", "10y", "", "USD", 30978, 30978 });
    cl.add({ "trade_22", "", "pf", ProductClass::Commodity, RiskType::CommodityVol, "Coal Americas", "1", "10y", "", "USD", -1812, -1812 });
    // clang-format on

    SimmCalculator simm(cl.netRecords(), config);
    SimmResults simmResults = simm.finalSimmResults(SimmSide::Call, NettingSetDetails("pf")).second;

    Real tol = 1.0E-6;
    Real margin3Ex = 0.0;
    for (const auto& pc : config->productClasses(false)) {
        map<RiskClass, Real> margin2ExComp;
        for (const auto& rc : config->riskClasses(false)) {
            // Manually aggregate over margin types
            Real margin1Ex = 0.0;
            for (const auto& mt : config->marginTypes(false)) {
                if (simmResults.has(pc, rc, mt, "All")) {
                    margin1Ex += simmResults.get(pc, rc, mt, "All");
                }
            }
            // What does the SimmCalculator give for the aggregate
            Real margin1 = 0.0;
            if (simmResults.has(pc, rc, MarginType::All, "All")) {
                margin1 = simmResults.get(pc, rc, MarginType::All, "All");
            }
            // Check
            if (std::abs(margin1 - margin1Ex) > tol)
                BOOST_ERROR("Failed to verify aggregation of margin types for "
                            << pc << ", " << rc << ", sum of delta, vega, curvature, baseCorr margin is " << margin1Ex
                            << ", computed value is " << margin1 << ", difference " << (margin1Ex - margin1)
                            << ", tolerance " << tol);
            margin2ExComp[rc] += margin1Ex;
        }

        // Manually aggregate over risk classes
        Real margin2Ex = 0.0;
        for (const auto& rco : config->riskClasses(false)) {
            for (const auto& rci : config->riskClasses(false)) {
                margin2Ex += config->correlationRiskClasses(rco, rci) * margin2ExComp.at(rco) * margin2ExComp.at(rci);
            }
        }
        margin2Ex = std::sqrt(margin2Ex);
        // What does the SimmCalculator give for the aggregate
        Real margin2 = 0.0;
        if (simmResults.has(pc, RiskClass::All, MarginType::All, "All")) {
            margin2 = simmResults.get(pc, RiskClass::All, MarginType::All, "All");
        }
        // Check
        if (std::abs(margin2 - margin2Ex) > tol)
            BOOST_ERROR("Failed to verify aggregation of margins over risk classes for product class "
                        << pc << ", expected value is " << margin2Ex << ", computed value is " << margin2
                        << ", difference " << (margin2Ex - margin2) << ", tolerance " << tol);
        margin3Ex += margin2Ex;
    }

    // What does the SimmCalculator give for the overall IM
    Real margin3 = 0.0;
    if (simmResults.has(ProductClass::All, RiskClass::All, MarginType::All, "All")) {
        margin3 = simmResults.get(ProductClass::All, RiskClass::All, MarginType::All, "All");
    }
    // Check
    if (std::abs(margin3 - margin3Ex) > tol)
        BOOST_ERROR("Failed to verify aggregation of margins over product classes, expected value is "
                    << margin3Ex << ", computed value is " << margin3 << ", difference " << (margin3Ex - margin3)
                    << ", tolerance " << tol);

    BOOST_CHECK(true);
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(SimmTest)

BOOST_AUTO_TEST_CASE(test1_0) {
    boost::shared_ptr<SimmBucketMapperBase> bucketMapper = boost::make_shared<SimmBucketMapperBase>("1.0");
    auto config = boost::make_shared<SimmConfiguration_ISDA_V1_0>(bucketMapper);
    boost::shared_ptr<Market> market = boost::make_shared<DummyMarket>();
    testIrDeltaMargin(config, market);
    testFxDeltaMargin(config, market);
    testCrqDeltaMargin(config, market);
    testCrnqDeltaMargin(config, market);
    testEqDeltaMargin(config, market);
    testComDeltaMargin(config, market);
    testIrVegaMargin(config, market);
    testFxVegaMargin(config, market);
    testCrqVegaMargin(config, market);
    testCrnqVegaMargin(config, market);
    testEqVegaMargin(config, market);
    testComVegaMargin(config, market);
    testIrCurvatureMargin(config, market);
    testFxCurvatureMargin(config, market);
    testCrqCurvatureMargin(config, market);
    testCrnqCurvatureMargin(config, market);
    testEqCurvatureMargin(config, market);
    testComCurvatureMargin(config, market);
    testMarginAggregation(config, market);
}

BOOST_AUTO_TEST_CASE(test1_3) {
    boost::shared_ptr<SimmBucketMapperBase> bucketMapper = boost::make_shared<SimmBucketMapperBase>("1.3");
    auto config = boost::make_shared<SimmConfiguration_ISDA_V1_3>(bucketMapper);
    boost::shared_ptr<Market> market = boost::make_shared<DummyMarket>();
    testIrDeltaMargin(config, market);
    testFxDeltaMargin(config, market);
    testCrqDeltaMargin(config, market);
    testCrnqDeltaMargin(config, market);
    testEqDeltaMargin(config, market);
    testComDeltaMargin(config, market);
    testIrVegaMargin(config, market);
    testFxVegaMargin(config, market);
    testCrqVegaMargin(config, market);
    testCrnqVegaMargin(config, market);
    testEqVegaMargin(config, market);
    testComVegaMargin(config, market);
    testIrCurvatureMargin(config, market);
    testFxCurvatureMargin(config, market);
    testCrqCurvatureMargin(config, market);
    testCrnqCurvatureMargin(config, market);
    testEqCurvatureMargin(config, market);
    testComCurvatureMargin(config, market);
    testMarginAggregation(config, market);

    // todo, add tests for the v1_3 specifics
    // - base correlation risk
    // - XCcy basis risk
    // - inflation vega risk
    // - non-unit concentration risk factors
    // - notional based add-on tests
    // note that the concentration risk factors are one in the 315 test cases
    // so that they pass even with the 329 configuration
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()


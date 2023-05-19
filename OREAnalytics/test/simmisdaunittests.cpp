/*
 Copyright (C) 2017 Quaternion Risk Management Ltd.
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
#include <iostream>
#include <ored/marketdata/dummymarket.hpp>
#include <orea/simm/crifloader.hpp>
#include <orea/simm/simmbucketmapperbase.hpp>
#include <orea/simm/simmcalculator.hpp>
#include <orea/simm/utilities.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/to_string.hpp>
#include <oret/datapaths.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>

using namespace ore::analytics;
using namespace ore::data;
using namespace boost::unit_test_framework;

using QuantLib::close_enough;
using QuantLib::Real;
using QuantLib::Size;
using std::set;
using std::string;
using std::vector;
using boost::filesystem::path;
using ore::data::to_string;

// Ease notation again
typedef SimmConfiguration::ProductClass ProductClass;
typedef SimmConfiguration::RiskClass RiskClass;
typedef SimmConfiguration::MarginType MarginType;
typedef SimmConfiguration::RiskType RiskType;

namespace {

struct SensitivityInput {
    std::string sensitivityId, productClass, riskType, qualifier, bucket, label1, label2;
    Real amount;
    std::string amountCurrency, collectRegulations;
};
struct SensitivityCombination {
    std::string combinationId, group, riskMeasure, elementOfCalculationTested, sensitivityIds, passesRequired;
    Real simmDelta, simmVega, simmCurvature, simmBaseCorr, simmAddOn, simmBenchmark;
};

// Ensure to call this with the expected value first if the expected value is exactly 0.0
bool check(const Real a, const Real b, const Real tol, Real absTol = 1e-6) {
    // If the expected value, a, is exactly zero then use absolute
    // tolerance to check the computed value, b.
    if (close_enough(a, 0.0))
        return std::fabs(b) < absTol;
    return std::fabs((b - a) / a) < tol;
}

void test(const string& version, const vector<SensitivityInput>& sensitivityInputs,
          const vector<SensitivityCombination>& sensitivityCombinations, const Size mporDays, const Real tol = 1E-12) {

    set<string> passed;

    boost::char_separator<char> sep(",");
    const string dummyTradeId = "DummyTradeId";
    const string dummyTradeType = "DummyTradeType";
    const NettingSetDetails dummyNettingSetDetails = NettingSetDetails("pf");

    for (auto const& sc : sensitivityCombinations) {
        BOOST_TEST_MESSAGE("Testing sensitivity combination: "
                           << sc.combinationId << ", group: " << sc.group << ", risk measure: " << sc.riskMeasure
                           << ", element of calculation tested: " << sc.elementOfCalculationTested << " (v" << version
                           << ").");

        // check if required cases have 
        boost::tokenizer<boost::char_separator<char>> tokens(sc.passesRequired, sep);
        bool passesRequired = true;
        for (auto const& r : tokens) {
            if (r == "None")
                continue;
            std::vector<std::string> tokensInn;
            boost::split(tokensInn, r, boost::is_any_of("-"));
            if (tokensInn.size() > 1) {
                if (tokensInn.size() != 2) {
                    passesRequired = false;
                    BOOST_TEST_MESSAGE("... can not interpret expression " << r
                                                                           << " as a list of required passed cases");
                }
                try {
                    Size num1 = boost::lexical_cast<Size>(tokensInn[0].substr(1, tokensInn[0].length() - 1));
                    Size num2 = boost::lexical_cast<Size>(tokensInn[1].substr(1, tokensInn[1].length() - 1));
                    for (Size num = num1; num <= num2 && passesRequired; ++num) {
                        std::ostringstream tmp;
                        tmp << tokensInn[0].substr(0, 1) << num;
                        if (std::find(passed.begin(), passed.end(), tmp.str()) == passed.end()) {
                            passesRequired = false;
                            BOOST_TEST_MESSAGE("... case " << tmp.str()
                                                           << " has not passed, which is required to run this case");
                        } else {
                            BOOST_TEST_MESSAGE("... case " << tmp.str() << " pass required: ok.");
                        }
                    }
                } catch (const std::exception& e) {
                    passesRequired = false;
                    BOOST_TEST_MESSAGE("... an error occured while parsing expression "
                                       << r << " as a list of required passed cases - " << e.what());
                }
            } else if (std::find(passed.begin(), passed.end(), r) == passed.end()) {
                passesRequired = false;
                BOOST_TEST_MESSAGE("... case " << r << " has not passed, which is required to run this case");
            } else {
                BOOST_TEST_MESSAGE("... case " << r << " pass required: ok.");
            }
            if (!passesRequired)
                break;
        }
        if (!passesRequired) {
            BOOST_ERROR("Skipping this case");
            continue;
        }

        // run the current case
        boost::shared_ptr<SimmBucketMapperBase> bucketMapper = boost::make_shared<SimmBucketMapperBase>(version);
        auto config = buildSimmConfiguration(version, bucketMapper, mporDays);
        CrifLoader cl(config, true);
        bool valid = true;

        boost::tokenizer<boost::char_separator<char>> tokens2(sc.sensitivityIds, sep);
        for (auto const& s : tokens2) {
            BOOST_TEST_MESSAGE("... feed input sensitivites " << s);
            auto sens = sensitivityInputs.begin();
            while ((sens = std::find_if(sens, sensitivityInputs.end(), [&s](const SensitivityInput& x) {
                        if ((s == "All" || s == "All S_") && x.sensitivityId.substr(0,2) == "S_")
                            return true;
                        
                        string base_s;
                        if (s.substr(0, 4) == "All ")
                            base_s = s.substr(4);
                        else
                            base_s = s;
                        
                        std::vector<std::string> tokens1, tokens2;
                        boost::split(tokens1, base_s, boost::is_any_of("_"));
                        boost::split(tokens2, x.sensitivityId, boost::is_any_of("_"));
                        for (Size j = 0; j < std::min(tokens1.size(), tokens2.size()); ++j) {
                            if (tokens1[j] != tokens2[j])
                                return false;
                        }
                        return true;
                    })) != sensitivityInputs.end()) {
                try {
                    const string collectRegulations = sens->collectRegulations == "All" ? "" : sens->collectRegulations;
                    CrifRecord cr = {dummyTradeId,
                                     dummyTradeType,
                                     dummyNettingSetDetails,
                                     parseSimmProductClass(sens->productClass),
                                     parseSimmRiskType(sens->riskType),
                                     sens->qualifier,
                                     sens->bucket,
                                     sens->label1,
                                     sens->label2,
                                     sens->amountCurrency,
                                     sens->amount,
                                     sens->amount,
                                     "SIMM",
                                     collectRegulations};

                    BOOST_TEST_MESSAGE("adding CRIF record " << cr);
                    cl.add(cr);
                } catch (const std::exception& e) {
                    BOOST_ERROR("An error occured: " << e.what() << ", skipping this case (v" << version << ").");
                    valid = false;
                    break;
                }
                ++sens;
            }
        }

        if (valid) {
            try {
                bool ok = true;
                boost::shared_ptr<Market> market = boost::make_shared<DummyMarket>();
                string simmCalcCcy = "USD";
                string simmResultCcy = "USD";
                SimmCalculator simm(cl.netRecords(true), config, simmCalcCcy, simmResultCcy, market, true, true);
                SimmResults simmResults = simm.finalSimmResults(SimmConfiguration::SimmSide::Call, dummyNettingSetDetails).second;

                ProductClass pc = ProductClass::All;
                RiskClass rc = RiskClass::All;

                MarginType mt = MarginType::Delta;
                Real m = simmResults.has(pc, rc, mt, "All") ? simmResults.get(pc, rc, mt, "All") : 0.0;
                BOOST_TEST_MESSAGE("... delta margin     (expected / got): " << sc.simmDelta << " " << m);
                ok = ok && check(sc.simmDelta, m, tol);

                mt = MarginType::Vega;
                m = simmResults.has(pc, rc, mt, "All") ? simmResults.get(pc, rc, mt, "All") : 0.0;
                BOOST_TEST_MESSAGE("... vega margin      (expected / got): " << sc.simmVega << " " << m);
                ok = ok && check(sc.simmVega, m, tol);

                mt = MarginType::Curvature;
                m = simmResults.has(pc, rc, mt, "All") ? simmResults.get(pc, rc, mt, "All") : 0.0;
                BOOST_TEST_MESSAGE("... curvature margin (expected / got): " << sc.simmCurvature << " " << m);
                ok = ok && check(sc.simmCurvature, m, tol);

                mt = MarginType::BaseCorr;
                m = simmResults.has(pc, rc, mt, "All") ? simmResults.get(pc, rc, mt, "All") : 0.0;
                BOOST_TEST_MESSAGE("... base corr margin (expected / got): " << sc.simmBaseCorr << " " << m);
                ok = ok && check(sc.simmBaseCorr, m, tol);

                mt = MarginType::AdditionalIM;
                m = simmResults.has(pc, rc, mt, "All") ? simmResults.get(pc, rc, mt, "All") : 0.0;
                BOOST_TEST_MESSAGE("... add on margin    (expected / got): " << sc.simmAddOn << " " << m);
                ok = ok && check(sc.simmAddOn, m, tol);
                mt = MarginType::All;
                m = simmResults.get(pc, rc, mt, "All");
                BOOST_TEST_MESSAGE("... total margin     (expected / got): " << sc.simmBenchmark << " " << m);
                ok = ok && check(sc.simmBenchmark, m, tol);

                if (ok) {
                    BOOST_TEST_MESSAGE("... passed (v" << version << ").");
                    passed.insert(sc.combinationId);
                } else {
                    BOOST_ERROR("Margin(s) could not be verified (v" << version << ", " << sc.combinationId << ", "
                                                                    << mporDays << " MPOR days).");
                }
            } catch (const std::exception& e) {
                BOOST_ERROR("An error occured: " << e.what() << " (v" << version << ", " << sc.combinationId << ", "
                                                 << mporDays << " MPOR days).");
            }
        }

    } // loop over sensitivity combinations
} // test

void test_csv(const string& version, const Size mporDays, const Real tol = 1E-12) {

    BOOST_TEST_MESSAGE("======================================================================");
    BOOST_TEST_MESSAGE("Running ISDA Unit Test Suite (for SIMM v" + version + ")");
    BOOST_TEST_MESSAGE("======================================================================");

    const string delim = "|";

    const string sensiInputPath = (TEST_INPUT_PATH / version / "sensitivity_inputs.csv").string();
    BOOST_TEST_MESSAGE("Loading sensitivity inputs for SIMM version v" << version << ".");
    CSVFileReader sensiInputReader(sensiInputPath, true, delim);
    const vector<string> expectedSensiInputHeaders({"Sensitivity_Id", "ProductClass", "RiskType", "Qualifier", "Bucket",
                                                    "Label1", "Label2", "Amount", "AmountCurrency"});
    for (const string& h : expectedSensiInputHeaders) {
        QL_REQUIRE(sensiInputReader.hasField(h), "Missing header \"" << h << "\" in file " << sensiInputPath);
    }
    const bool hasRegulations = sensiInputReader.hasField("CollectRegulations");

    vector<SensitivityInput> sensitivityInputs;
    string regulation;
    while (sensiInputReader.next()) {
        regulation = hasRegulations ? sensiInputReader.get("CollectRegulations") : "";
        SensitivityInput si = {sensiInputReader.get("Sensitivity_Id"), sensiInputReader.get("ProductClass"),
                               sensiInputReader.get("RiskType"),       sensiInputReader.get("Qualifier"),
                               sensiInputReader.get("Bucket"),         sensiInputReader.get("Label1"),
                               sensiInputReader.get("Label2"),         parseReal(sensiInputReader.get("Amount")),
                               sensiInputReader.get("AmountCurrency"), regulation};
        sensitivityInputs.push_back(si);
    }
    sensiInputReader.close();

    const string sensiCombPath =
        (TEST_INPUT_PATH / version / ("sensitivity_combinations_" + to_string(mporDays) + ".csv")).string();
    BOOST_TEST_MESSAGE("Loading sensitivity combinations for SIMM version v" << version << " (MPOR days = " << mporDays
                                                                            << ").");
    CSVFileReader sensiCombReader(sensiCombPath, true, delim);
    vector<string> expectedSensiCombHeaders({"Combination Id", "Group", "Risk Measure", "Element of Calculation Tested",
                                             "Sensitivity Ids", "Passes Required", "SIMM Delta", "SIMM Vega",
                                             "SIMM Curvature", "SIMM Base Corr", "SIMM AddOn", "SIMM Benchmark"});
    for (const string& h : expectedSensiCombHeaders) {
        QL_REQUIRE(sensiCombReader.hasField(h), "Missing header \"" << h << "\" in file " << sensiCombPath);
    }
    vector<SensitivityCombination> sensitivityCombinations;
    while (sensiCombReader.next()) {
        SensitivityCombination sc = {sensiCombReader.get("Combination Id"),
                                     sensiCombReader.get("Group"),
                                     sensiCombReader.get("Risk Measure"),
                                     sensiCombReader.get("Element of Calculation Tested"),
                                     sensiCombReader.get("Sensitivity Ids"),
                                     sensiCombReader.get("Passes Required"),
                                     parseReal(sensiCombReader.get("SIMM Delta")),
                                     parseReal(sensiCombReader.get("SIMM Vega")),
                                     parseReal(sensiCombReader.get("SIMM Curvature")),
                                     parseReal(sensiCombReader.get("SIMM Base Corr")),
                                     parseReal(sensiCombReader.get("SIMM AddOn")),
                                     parseReal(sensiCombReader.get("SIMM Benchmark"))};
        sensitivityCombinations.push_back(sc);
    }
    sensiCombReader.close();

     test(version, sensitivityInputs, sensitivityCombinations, mporDays, tol);

} // test_csv
} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(SimmIsdaUnitTests)

// ============================================================================
// Source: ISDA-SIMM-UnitTesting-Benchmark-v30.xlsx, for testing of v1.3
// ============================================================================
BOOST_AUTO_TEST_CASE(test1_3) {
    test_csv("1.3", 10);
    BOOST_CHECK(true);
}

// ============================================================================
// Source: ISDA-SIMM-UnitTesting-Benchmark-10d-v38r1.xlsx, for testing of v1.3.38
// ============================================================================
BOOST_AUTO_TEST_CASE(test1_3_38) {
    test_csv("1.3.38", 10);
    BOOST_CHECK(true);
}

// ============================================================================
// Source: ISDA-SIMM-UnitTesting-Benchmark-10d-v44r0.xlsx, for testing of v2.0
// ============================================================================
BOOST_AUTO_TEST_CASE(test2_0) {
    test_csv("2.0", 10);
    BOOST_CHECK(true);
}

// ============================================================================================
// Source: ISDA-SIMM-UnitTesting-Benchmark-10d-v2.1_(same-as-v2.0.6).xlsx, for testing of v2_1
// ============================================================================================
BOOST_AUTO_TEST_CASE(test2_1) {
    test_csv("2.1", 10);
    BOOST_CHECK(true);
}


// ============================================================================================
// Source: ISDA-SIMM-UnitTesting-Benchmark-v2.16r2.xlsx, for testing of v2_2
// ============================================================================================
BOOST_AUTO_TEST_CASE(test2_2) {
    test_csv("2.2", 10);
    BOOST_CHECK(true);
    test_csv("2.2", 1);
    BOOST_CHECK(true);
}

// ============================================================================================
// Source: ISDA-SIMM-UnitTesting-Benchmark-v2.3.xlsx, for testing of v2_3
// ============================================================================================
BOOST_AUTO_TEST_CASE(test2_3) {
    test_csv("2.3", 10);
    BOOST_CHECK(true);
    test_csv("2.3", 1);
    BOOST_CHECK(true);
}

// ============================================================================================
// Source: ISDA-SIMM-UnitTesting-Benchmark-v2.3.8.xlsx, for testing of v2_3_8
// ============================================================================================
BOOST_AUTO_TEST_CASE(test2_3_8) {
    test_csv("2.3.8", 10);
    BOOST_CHECK(true);
    test_csv("2.3.8", 1);
    BOOST_CHECK(true);
}

// ============================================================================================
// Source: ISDA-SIMM-UnitTesting-v2.5.xlsx, for testing of v2_5
// ============================================================================================
BOOST_AUTO_TEST_CASE(test2_5) {
    test_csv("2.5", 10);
    BOOST_CHECK(true);
    test_csv("2.5", 1);
    BOOST_CHECK(true);
}

// ============================================================================================
// Source: ISDA-SIMM-UnitTesting-v2.5.a.xlsx, for testing of v2_5A
// ============================================================================================
BOOST_AUTO_TEST_CASE(test2_5A) {
    test_csv("2.5A", 10);
    BOOST_CHECK(true);
    test_csv("2.5A", 1);
    BOOST_CHECK(true);
}


BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()


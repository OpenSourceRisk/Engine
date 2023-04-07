/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <oret/toplevelfixture.hpp>
#include <test/testmarket.hpp>
#include <orea/scenario/simplescenario.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/scenario/historicalscenariogenerator.hpp>

using namespace std;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(HistoricalScenarioGeneratorTest)

BOOST_AUTO_TEST_CASE(testHistoricalScenarioGeneratorTransform) {

    BOOST_TEST_MESSAGE(
        "Checking transformation of discount factors to zero rates in Historical Scenario Generator Transform...");

    vector<boost::shared_ptr<Scenario>> scenarios;

    // Make up some scenarios
    Date d1 = Date(14, April, 2016);
    Date d2 = d1 + Period(1, Days);
    Settings::instance().evaluationDate() = d2;

    map<RiskFactorKey, Real> rfks = {{{RiskFactorKey::KeyType::DiscountCurve, "EUR", 3}, 0.999},
                                     {{RiskFactorKey::KeyType::DiscountCurve, "EUR", 4}, 0.995},
                                     {{RiskFactorKey::KeyType::DiscountCurve, "EUR", 5}, 0.99},
                                     {{RiskFactorKey::KeyType::DiscountCurve, "EUR", 6}, 0.981},
                                     {{RiskFactorKey::KeyType::IndexCurve, "EUR-EURIBOR-6M", 3}, 0.997},
                                     {{RiskFactorKey::KeyType::IndexCurve, "EUR-EURIBOR-6M", 4}, 0.985},
                                     {{RiskFactorKey::KeyType::IndexCurve, "EUR-EURIBOR-6M", 5}, 0.979},
                                     {{RiskFactorKey::KeyType::IndexCurve, "EUR-EURIBOR-6M", 6}, 0.965},
                                     {{RiskFactorKey::KeyType::SurvivalProbability, "dc", 1}, 0.920},
                                     {{RiskFactorKey::KeyType::SurvivalProbability, "dc", 2}, 0.905},
                                     {{RiskFactorKey::KeyType::SurvivalProbability, "dc", 3}, 0.875},
                                     {{RiskFactorKey::KeyType::SurvivalProbability, "dc", 4}, 0.861}};

    boost::shared_ptr<Scenario> s1 = boost::make_shared<SimpleScenario>(d1);
    boost::shared_ptr<Scenario> s2 = boost::make_shared<SimpleScenario>(d2);

    for (auto rf : rfks) {
        s1->add(rf.first, 1);
        s2->add(rf.first, rf.second);
    }

    scenarios.push_back(s1);
    scenarios.push_back(s2);
    boost::shared_ptr<HistoricalScenarioLoader> histScenariosLoader = boost::make_shared<HistoricalScenarioLoader>();
    histScenariosLoader->historicalScenarios() = scenarios;
    histScenariosLoader->dates() = vector<Date>{d1, d2};
    boost::shared_ptr<HistoricalScenarioGenerator> histScenarios = boost::make_shared<HistoricalScenarioGenerator>(
        histScenariosLoader, boost::make_shared<SimpleScenarioFactory>(), TARGET(), 1);
    histScenarios->baseScenario() = s1;

    // Init market
    testsuite::TestConfigurationObjects::setConventions();
    boost::shared_ptr<Market> initMarket = boost::make_shared<testsuite::TestMarketParCurves>(d2);

    // build scen sim market params
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData =
        testsuite::TestConfigurationObjects::setupSimMarketData();

    // build scenario sim market
    boost::shared_ptr<analytics::ScenarioSimMarket> simMarket =
        boost::make_shared<analytics::ScenarioSimMarket>(initMarket, simMarketData);

    boost::shared_ptr<HistoricalScenarioGeneratorTransform> histScenTrasform =
        boost::make_shared<HistoricalScenarioGeneratorTransform>(histScenarios, simMarket, simMarketData);
    histScenTrasform->reset();
    boost::shared_ptr<Scenario> scenarioTransform = histScenTrasform->next(d2);

    DayCounter dc;
    vector<Period> tenors;
    Real tolerance = 0.0001;
    for (auto rfk : rfks) {
        auto rf = rfk.first;
        if (rf.keytype == RiskFactorKey::KeyType::DiscountCurve) {
            dc = simMarket->discountCurve(rf.name)->dayCounter();
            tenors = simMarketData->yieldCurveTenors(rf.name);
        } else if (rf.keytype == RiskFactorKey::KeyType::IndexCurve) {
            dc = simMarket->iborIndex(rf.name)->dayCounter();
            tenors = simMarketData->yieldCurveTenors(rf.name);
        } else if (rf.keytype == RiskFactorKey::KeyType::SurvivalProbability) {
            dc = simMarket->defaultCurve(rf.name)->curve()->dayCounter();
            tenors = simMarketData->defaultTenors(rf.name);
        }
        Time t = dc.yearFraction(d2, d2 + tenors[rf.index]);
        Real expected = std::log(1 / rfk.second) / t;
        Real transform = scenarioTransform->get(rf);
        BOOST_CHECK_MESSAGE(fabs(expected - transform) < tolerance,
                            "Difference between expected and transformed rate > than " << tolerance << " for key "
                                                                                       << rf.name);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

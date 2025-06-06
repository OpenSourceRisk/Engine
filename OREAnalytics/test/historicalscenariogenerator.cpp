/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <oret/toplevelfixture.hpp>
#include <orea/scenario/simplescenario.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/scenario/historicalscenariogenerator.hpp>

#include "testmarket.hpp"

using namespace std;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(HistoricalReturnConfiguration)

BOOST_AUTO_TEST_CASE(testHistoricalReturnConfiguration) {
    using ore::analytics::ReturnConfiguration;
    using ore::analytics::RiskFactorKey;

    // Testdata
    Real v1 = 2.0;
    Real v2 = 1.0;
    Date d1 = Date(1, Jan, 2020);
    Date d2 = Date(2, Jan, 2020);

    // 1. Default constructor should be log for discount curve
    ReturnConfiguration defaultConfig;
    RiskFactorKey keyAbs(RiskFactorKey::KeyType::DiscountCurve, "EUR", 0);
    Real absReturn = defaultConfig.returnValue(keyAbs, v1, v2, d1, d2);
    BOOST_CHECK_CLOSE(absReturn, std::log(v2 / v1), 1e-12);

    // 2. Configs with displacement
    std::map<RiskFactorKey::KeyType, ReturnConfiguration::RiskFactorConfig> configs;

    // Absolute
    ReturnConfiguration::Return absRet{ReturnConfiguration::ReturnType::Absolute, 0.0};
    configs[RiskFactorKey::KeyType::DiscountCurve] =
        std::make_pair(absRet, ReturnConfiguration::IndividualRiskFactorConfig{});

    // Relative
    ReturnConfiguration::Return relRet{ReturnConfiguration::ReturnType::Relative, 0.5};
    configs[RiskFactorKey::KeyType::IndexCurve] =
        std::make_pair(relRet, ReturnConfiguration::IndividualRiskFactorConfig{});

    // Log
    ReturnConfiguration::Return logRet{ReturnConfiguration::ReturnType::Log, 0.1};
    configs[RiskFactorKey::KeyType::SurvivalProbability] =
        std::make_pair(logRet, ReturnConfiguration::IndividualRiskFactorConfig{});

    // 3. Configs with a specialized config for crude oil with displacement
    ReturnConfiguration::Return relRetDefault{ReturnConfiguration::ReturnType::Relative, 0.0};
    ReturnConfiguration::Return relRetSpecial{ReturnConfiguration::ReturnType::Relative, 0.7};
    ReturnConfiguration::IndividualRiskFactorConfig relSpecialized;
    relSpecialized["WTI"] = relRetSpecial;
    configs[RiskFactorKey::KeyType::CommodityCurve] = std::make_pair(relRetDefault, relSpecialized);

    ReturnConfiguration config2(configs);

    // Absolute
    RiskFactorKey keyAbs2(RiskFactorKey::KeyType::DiscountCurve, "EUR", 0);
    Real absReturn2 = config2.returnValue(keyAbs2, v1, v2, d1, d2);
    BOOST_CHECK_CLOSE(absReturn2, v2 - v1, 1e-12);

    // Relative
    RiskFactorKey keyRel(RiskFactorKey::KeyType::IndexCurve, "USD", 0);
    Real relReturn = config2.returnValue(keyRel, v1, v2, d1, d2);
    Real expectedRel = (v2 + 0.5) / (v1 + 0.5) - 1.0;
    BOOST_CHECK_CLOSE(relReturn, expectedRel, 1e-12);

    // Log
    RiskFactorKey keyLog(RiskFactorKey::KeyType::SurvivalProbability, "dc", 0);
    Real logReturn = config2.returnValue(keyLog, v1, v2, d1, d2);
    Real expectedLog = std::log((v2 + 0.1) / (v1 + 0.1));
    BOOST_CHECK_CLOSE(logReturn, expectedLog, 1e-12);

    RiskFactorKey keyRelSpec(RiskFactorKey::KeyType::CommodityCurve, "Brent", 0);
    Real relReturnSpec = config2.returnValue(keyRelSpec, v1, v2, d1, d2);
    Real expectedRelSpec = (v2 + 0.0) / (v1 + 0.0) - 1.0;
    BOOST_CHECK_CLOSE(relReturnSpec, expectedRelSpec, 1e-12);

    RiskFactorKey keyRelDefault(RiskFactorKey::KeyType::CommodityCurve, "WTI", 0);
    Real relReturnDefault = config2.returnValue(keyRelDefault, v1, v2, d1, d2);
    Real expectedRelDefault = (v2 + 0.7) / (v1 + 0.7) - 1.0;
    BOOST_CHECK_CLOSE(relReturnDefault, expectedRelDefault, 1e-12);
}

BOOST_AUTO_TEST_CASE(testHistoricalReturnConfigurationFromXML) {
    using ore::analytics::ReturnConfiguration;
    using ore::analytics::RiskFactorKey;

    std::string xml = R"(
    <ReturnConfigurations>
        <ReturnConfiguration key="CommodityCurve">
            <Return>
                <Type>Relative</Type>
                <Displacement>0.0</Displacement>
            </Return>
            <SpecializedConfigurations>
                <Return key="WTI">
                    <Type>Relative</Type>
                    <Displacement>0.7</Displacement>
                </Return>
            </SpecializedConfigurations>
        </ReturnConfiguration>
        <ReturnConfiguration key="DiscountCurve">
            <Return>
                <Type>Absolute</Type>
                <Displacement>0.0</Displacement>
            </Return>
        </ReturnConfiguration>
    </ReturnConfigurations>
    )";

    ReturnConfiguration config;
    config.fromXMLString(xml);

    Real v1 = 2.0;
    Real v2 = 1.0;
    Date d1 = Date(1, Jan, 2020);
    Date d2 = Date(2, Jan, 2020);

    RiskFactorKey keyWTI(RiskFactorKey::KeyType::CommodityCurve, "WTI", 0);
    Real relReturnWTI = config.returnValue(keyWTI, v1, v2, d1, d2);
    Real expectedRelWTI = (v2 + 0.7) / (v1 + 0.7) - 1.0;
    BOOST_CHECK_CLOSE(relReturnWTI, expectedRelWTI, 1e-12);

    RiskFactorKey keyBrent(RiskFactorKey::KeyType::CommodityCurve, "Brent", 0);
    Real relReturnBrent = config.returnValue(keyBrent, v1, v2, d1, d2);
    Real expectedRelBrent = (v2 + 0.0) / (v1 + 0.0) - 1.0;
    BOOST_CHECK_CLOSE(relReturnBrent, expectedRelBrent, 1e-12);

    RiskFactorKey keyDisc(RiskFactorKey::KeyType::DiscountCurve, "EUR", 0);
    Real absReturn = config.returnValue(keyDisc, v1, v2, d1, d2);
    Real expectedAbs = v2 - v1;
    BOOST_CHECK_CLOSE(absReturn, expectedAbs, 1e-12);
}

BOOST_AUTO_TEST_CASE(testHistoricalReturnConfigurationXmlRoundtrip) {
    using ore::analytics::ReturnConfiguration;
    using ore::analytics::RiskFactorKey;

    std::map<RiskFactorKey::KeyType, ReturnConfiguration::RiskFactorConfig> configs;

    ReturnConfiguration::Return absRet{ReturnConfiguration::ReturnType::Absolute, 0.0};
    configs[RiskFactorKey::KeyType::DiscountCurve] =
        std::make_pair(absRet, ReturnConfiguration::IndividualRiskFactorConfig{});

    ReturnConfiguration::Return relRetDefault{ReturnConfiguration::ReturnType::Relative, 0.0};
    ReturnConfiguration::Return relRetSpecial{ReturnConfiguration::ReturnType::Relative, 0.7};
    ReturnConfiguration::IndividualRiskFactorConfig relSpecialized;
    relSpecialized["WTI"] = relRetSpecial;
    configs[RiskFactorKey::KeyType::CommodityCurve] = std::make_pair(relRetDefault, relSpecialized);

    ReturnConfiguration config1(configs);

    ore::data::XMLDocument doc;
    auto* xmlNode = config1.toXML(doc);
    doc.appendNode(xmlNode);
    std::string xmlString = doc.toString();

    ore::data::XMLDocument doc2;
    doc2.fromXMLString(xmlString);
    XMLNode* root = doc2.getFirstNode("ReturnConfigurations");
    ReturnConfiguration config2;
    config2.fromXML(root);

    Real v1 = 2.0;
    Real v2 = 1.0;
    Date d1 = Date(1, Jan, 2020);
    Date d2 = Date(2, Jan, 2020);

    RiskFactorKey keyDisc(RiskFactorKey::KeyType::DiscountCurve, "EUR", 0);
    BOOST_CHECK_CLOSE(config1.returnValue(keyDisc, v1, v2, d1, d2), config2.returnValue(keyDisc, v1, v2, d1, d2),
                      1e-12);

    RiskFactorKey keyBrent(RiskFactorKey::KeyType::CommodityCurve, "Brent", 0);
    BOOST_CHECK_CLOSE(config1.returnValue(keyBrent, v1, v2, d1, d2), config2.returnValue(keyBrent, v1, v2, d1, d2),
                      1e-12);

    RiskFactorKey keyWTI(RiskFactorKey::KeyType::CommodityCurve, "WTI", 0);
    BOOST_CHECK_CLOSE(config1.returnValue(keyWTI, v1, v2, d1, d2), config2.returnValue(keyWTI, v1, v2, d1, d2), 1e-12);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(HistoricalScenarioGeneratorTest)

BOOST_AUTO_TEST_CASE(testHistoricalScenarioGeneratorTransform) {

    BOOST_TEST_MESSAGE(
        "Checking transformation of discount factors to zero rates in Historical Scenario Generator Transform...");

    vector<map<Date, QuantLib::ext::shared_ptr<Scenario>>> scenarios;
    map<Date, QuantLib::ext::shared_ptr<Scenario>> scenarioMap;
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

    QuantLib::ext::shared_ptr<Scenario> s1 = QuantLib::ext::make_shared<SimpleScenario>(d1);
    QuantLib::ext::shared_ptr<Scenario> s2 = QuantLib::ext::make_shared<SimpleScenario>(d2);

    for (auto rf : rfks) {
        s1->add(rf.first, 1);
        s2->add(rf.first, rf.second);
    }

    scenarioMap[d1] = s1;
    scenarioMap[d2] = s2;
    scenarios.push_back(scenarioMap);
    QuantLib::ext::shared_ptr<HistoricalScenarioLoader> histScenariosLoader = QuantLib::ext::make_shared<HistoricalScenarioLoader>();
    histScenariosLoader->scenarios() = scenarios;
    histScenariosLoader->dates() = vector<Date>{d1, d2};
    QuantLib::ext::shared_ptr<HistoricalScenarioGenerator> histScenarios = QuantLib::ext::make_shared<HistoricalScenarioGenerator>(
        histScenariosLoader, QuantLib::ext::make_shared<SimpleScenarioFactory>(true), TARGET(), nullptr, 1);
    histScenarios->baseScenario() = s1;

    // Init market
    testsuite::TestConfigurationObjects::setConventions();
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<testsuite::TestMarketParCurves>(d2);

    // build scen sim market params
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData =
        testsuite::TestConfigurationObjects::setupSimMarketData();

    // build scenario sim market
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarket> simMarket =
        QuantLib::ext::make_shared<analytics::ScenarioSimMarket>(initMarket, simMarketData);

    QuantLib::ext::shared_ptr<HistoricalScenarioGeneratorTransform> histScenTrasform =
        QuantLib::ext::make_shared<HistoricalScenarioGeneratorTransform>(histScenarios, simMarket, simMarketData);
    histScenTrasform->reset();
    QuantLib::ext::shared_ptr<Scenario> scenarioTransform = histScenTrasform->next(d2);

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

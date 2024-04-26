/*
Copyright (C) 2016 Quaternion Risk Management Ltd
All rights reserved.
*/

#include <oret/toplevelfixture.hpp>
#include <boost/make_shared.hpp>
#include <orea/scenario/scenariowriter.hpp>
#include <orea/scenario/simplescenario.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/scenario/csvscenariogenerator.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace ore::analytics;

class TestScenarioGenerator : public ScenarioGenerator {
public:
    TestScenarioGenerator() { current_position_ = 0; }
    void addScenario(QuantLib::ext::shared_ptr<Scenario> s) {
        scenarios.push_back(s);
        current_position_++;
    }

    QuantLib::ext::shared_ptr<Scenario> next(const Date& d) override { return scenarios[current_position_++]; }

    void reset() override { current_position_ = 0; }

    vector<QuantLib::ext::shared_ptr<Scenario>> scenarios;

private:
    int current_position_;
};

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CSVScenarioGeneratorTest)

BOOST_AUTO_TEST_CASE(testCSVScenarioGenerator) {

    // Make up some scenarios
    Date d(21, Dec, 2016);
    vector<RiskFactorKey> rfks = {{RiskFactorKey::KeyType::DiscountCurve, "CHF", 0},
                                  {RiskFactorKey::KeyType::DiscountCurve, "CHF", 1},
                                  {RiskFactorKey::KeyType::DiscountCurve, "CHF", 2},
                                  {RiskFactorKey::KeyType::YieldCurve, "CHF-LIBOR", 0},
                                  {RiskFactorKey::KeyType::YieldCurve, "CHF-LIBOR", 1},
                                  {RiskFactorKey::KeyType::IndexCurve, "CHF - LIBOR - 6M", 0},
                                  {RiskFactorKey::KeyType::IndexCurve, "CHF - LIBOR - 6M", 1},
                                  {RiskFactorKey::KeyType::IndexCurve, "CHF - LIBOR - 6M", 2},
                                  {RiskFactorKey::KeyType::SwaptionVolatility, "SwapVol"},
                                  {RiskFactorKey::KeyType::FXSpot, "CHF"},
                                  {RiskFactorKey::KeyType::FXVolatility, "CHFVol"}};

    QuantLib::ext::shared_ptr<TestScenarioGenerator> tsg = QuantLib::ext::make_shared<TestScenarioGenerator>();
    tsg->addScenario(QuantLib::ext::make_shared<SimpleScenario>(d));
    tsg->addScenario(QuantLib::ext::make_shared<SimpleScenario>(d));
    for (auto scenario : tsg->scenarios) {
        for (auto rf : rfks) {
            scenario->add(rf, rand());
        }
    }

    // Write scenarios to file
    string filename = "test_csv_scenario_generator.csv";
    ScenarioWriter sw(tsg, filename);
    tsg->reset();
    for (Size i = 0; i < tsg->scenarios.size(); i++) {
        sw.next(d);
    }
    sw.reset();

    // Read in scenarios from file
    QuantLib::ext::shared_ptr<SimpleScenarioFactory> ssf = QuantLib::ext::make_shared<SimpleScenarioFactory>(true);
    CSVScenarioGenerator csvsgen(filename, ssf);

    // Compare scenarios by looping over risk keys
    for (Size i = 0; i < tsg->scenarios.size(); i++) {
        QuantLib::ext::shared_ptr<Scenario> s = csvsgen.next(d);
        BOOST_CHECK_EQUAL_COLLECTIONS(s->keys().begin(), s->keys().end(), tsg->scenarios[i]->keys().begin(),
                                      tsg->scenarios[i]->keys().end());
        for (auto rfk : s->keys()) {
            BOOST_CHECK_EQUAL(s->get(rfk), tsg->scenarios[i]->get(rfk));
        }
    }

    remove("test_csv_scenario_generator.csv");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

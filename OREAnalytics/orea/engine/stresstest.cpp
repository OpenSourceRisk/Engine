/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <orea/cube/inmemorycube.hpp>
#include <orea/engine/stresstest.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/scenario/clonescenariofactory.hpp>

#include <ored/utilities/log.hpp>

#include <ql/errors.hpp>

#include <boost/lexical_cast.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using namespace ore::data;

namespace ore {
namespace analytics {

StressTest::StressTest(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                       const QuantLib::ext::shared_ptr<ore::data::Market>& market, const string& marketConfiguration,
                       const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
                       const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                       const QuantLib::ext::shared_ptr<StressTestScenarioData>& stressData,
                       const CurveConfigurations& curveConfigs, const TodaysMarketParameters& todaysMarketParams,
                       QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory,
                       const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                       const IborFallbackConfig& iborFallbackConfig, bool continueOnError) {

    LOG("Run Stress Test");
    DLOG("Build Simulation Market");
    QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        market, simMarketData, marketConfiguration, curveConfigs, todaysMarketParams, continueOnError,
        stressData->useSpreadedTermStructures(), false, false, iborFallbackConfig, true);

    DLOG("Build Stress Scenario Generator");
    Date asof = market->asofDate();
    QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket->baseScenario();
    scenarioFactory = scenarioFactory ? scenarioFactory : QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);
    QuantLib::ext::shared_ptr<StressScenarioGenerator> scenarioGenerator = QuantLib::ext::make_shared<StressScenarioGenerator>(
        stressData, baseScenario, simMarketData, simMarket, scenarioFactory, simMarket->baseScenarioAbsolute());
    simMarket->scenarioGenerator() = scenarioGenerator;

    DLOG("Build Engine Factory");
    map<MarketContext, string> configurations;
    configurations[MarketContext::pricing] = marketConfiguration;
    auto ed = QuantLib::ext::make_shared<EngineData>(*engineData);
    ed->globalParameters()["RunType"] = "Stress";
    QuantLib::ext::shared_ptr<EngineFactory> factory =
    QuantLib::ext::make_shared<EngineFactory>(ed, simMarket, configurations, referenceData, iborFallbackConfig);

    DLOG("Reset and Build Portfolio");
    portfolio->reset();
    portfolio->build(factory, "stress analysis");

    DLOG("Build the cube object to store sensitivities");
    QuantLib::ext::shared_ptr<NPVCube> cube = QuantLib::ext::make_shared<DoublePrecisionInMemoryCube>(
        asof, portfolio->ids(), vector<Date>(1, asof), scenarioGenerator->samples());

    DLOG("Run Stress Scenarios");
    QuantLib::ext::shared_ptr<DateGrid> dg = QuantLib::ext::make_shared<DateGrid>("1,0W", NullCalendar());
    vector<QuantLib::ext::shared_ptr<ValuationCalculator>> calculators;
    calculators.push_back(QuantLib::ext::make_shared<NPVCalculator>(simMarketData->baseCcy()));
    ValuationEngine engine(asof, dg, simMarket, factory->modelBuilders());

    engine.registerProgressIndicator(QuantLib::ext::make_shared<ProgressLog>("stress scenarios", 100, oreSeverity::notice));
    engine.buildCube(portfolio, cube, calculators);

    /*****************
     * Collect results
     */
    baseNPV_.clear();
    shiftedNPV_.clear();
    delta_.clear();
    labels_.clear();
    trades_.clear();
    for (auto const& [tradeId, trade] : portfolio->trades()) {
        auto index = cube->idsAndIndexes().find(tradeId);
        if (index == cube->idsAndIndexes().end()) {
            ALOG("cube does not contain tradeId '" << tradeId << "'");
            continue;
        }
        Real npv0 = cube->getT0(index->second, 0);
        trades_.insert(tradeId);
        baseNPV_[tradeId] = npv0;
        for (Size j = 0; j < scenarioGenerator->samples(); ++j) {
            const string& label = scenarioGenerator->scenarios()[j]->label();
            TLOG("Adding stress test result for trade '" << tradeId << "' and scenario #" << j << " '" << label << "'");
            Real npv = cube->get(index->second, 0, j, 0);
            pair<string, string> p(tradeId, label);
            shiftedNPV_[p] = npv;
            delta_[p] = npv - npv0;
            labels_.insert(label);
        }
    }
    LOG("Stress testing done");
}

void StressTest::writeReport(const QuantLib::ext::shared_ptr<ore::data::Report>& report, Real outputThreshold) {

    report->addColumn("TradeId", string());
    report->addColumn("ScenarioLabel", string());
    report->addColumn("Base NPV", double(), 2);
    report->addColumn("Scenario NPV", double(), 2);
    report->addColumn("Sensitivity", double(), 2);

    for (auto const& [id, npv] : shiftedNPV_) {
        string tradeId = id.first;
        string factor = id.second;
        Real base = baseNPV_[tradeId];
        Real sensi = npv - base;
        TLOG("Adding stress report result for tradeId '" << tradeId << "' and scenario '" << factor << ": sensi = "
                                                         << sensi << ", threshold = " << outputThreshold);
        if (fabs(sensi) > outputThreshold || QuantLib::close_enough(sensi, outputThreshold)) {
            report->next();
            report->add(tradeId);
            report->add(factor);
            report->add(base);
            report->add(npv);
            report->add(sensi);
        }
    }

    report->end();
}
} // namespace analytics
} // namespace ore

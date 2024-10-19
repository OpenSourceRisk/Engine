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
#include <orea/engine/cashflowreportgenerator.hpp>
#include <orea/engine/stresstest.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/scenario/clonescenariofactory.hpp>

#include <ored/utilities/log.hpp>

#include <ql/errors.hpp>

#include <boost/lexical_cast.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;

namespace ore {
namespace analytics {

void runStressTest(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                   const QuantLib::ext::shared_ptr<ore::data::Market>& market, const string& marketConfiguration,
                   const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
                   const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                   const QuantLib::ext::shared_ptr<StressTestScenarioData>& stressData,
                   const boost::shared_ptr<ore::data::Report>& report,
                   const boost::shared_ptr<ore::data::Report>& cfReport, const double threshold, const Size precision,
                   const bool includePastCashflows, const CurveConfigurations& curveConfigs,
                   const TodaysMarketParameters& todaysMarketParams,
                   QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory,
                   const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                   const IborFallbackConfig& iborFallbackConfig, bool continueOnError) {

    LOG("Run Stress Test");

    QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        market, simMarketData, marketConfiguration, curveConfigs, todaysMarketParams, continueOnError,
        stressData->useSpreadedTermStructures(), false, false, iborFallbackConfig, true);

    Date asof = market->asofDate();
    QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket->baseScenario();
    scenarioFactory =
        scenarioFactory ? scenarioFactory : QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);
    QuantLib::ext::shared_ptr<StressScenarioGenerator> scenarioGenerator =
        QuantLib::ext::make_shared<StressScenarioGenerator>(stressData, baseScenario, simMarketData, simMarket,
                                                            scenarioFactory, simMarket->baseScenarioAbsolute());
    simMarket->scenarioGenerator() = scenarioGenerator;

    map<MarketContext, string> configurations;
    configurations[MarketContext::pricing] = marketConfiguration;
    auto ed = QuantLib::ext::make_shared<EngineData>(*engineData);
    ed->globalParameters()["RunType"] = "Stress";
    QuantLib::ext::shared_ptr<EngineFactory> factory =
        QuantLib::ext::make_shared<EngineFactory>(ed, simMarket, configurations, referenceData, iborFallbackConfig);

    portfolio->reset();
    portfolio->build(factory, "stress analysis");

    QuantLib::ext::shared_ptr<NPVCube> cube = QuantLib::ext::make_shared<DoublePrecisionInMemoryCube>(
        asof, portfolio->ids(), vector<Date>(1, asof), scenarioGenerator->samples());

    std::vector<std::vector<std::vector<TradeCashflowReportData>>> cfCube;
    if (cfReport)
        cfCube = std::vector<std::vector<std::vector<TradeCashflowReportData>>>(
            portfolio->ids().size(),
            std::vector<std::vector<TradeCashflowReportData>>(scenarioGenerator->samples() + 1));

    QuantLib::ext::shared_ptr<DateGrid> dg = QuantLib::ext::make_shared<DateGrid>("1,0W", NullCalendar());
    vector<QuantLib::ext::shared_ptr<ValuationCalculator>> calculators;
    calculators.push_back(QuantLib::ext::make_shared<NPVCalculator>(simMarketData->baseCcy()));
    if (cfReport) {
        calculators.push_back(QuantLib::ext::make_shared<CashflowReportCalculator>(simMarketData->baseCcy(),
                                                                                   includePastCashflows, cfCube));
    }
    ValuationEngine engine(asof, dg, simMarket, factory->modelBuilders());

    engine.registerProgressIndicator(
        QuantLib::ext::make_shared<ProgressLog>("stress scenarios", 100, oreSeverity::notice));
    engine.buildCube(portfolio, cube, calculators, ValuationEngine::ErrorPolicy::RemoveSample);

    report->addColumn("TradeId", string());
    report->addColumn("ScenarioLabel", string());
    report->addColumn("Base NPV", double(), precision);
    report->addColumn("Scenario NPV", double(), precision);
    report->addColumn("Sensitivity", double(), precision);

    if (cfReport) {
        cfReport->addColumn("TradeId", string());
        cfReport->addColumn("ScenarioLabel", string());
        cfReport->addColumn("Base Amount", double(), precision);
        cfReport->addColumn("Scenario Amount", double(), precision);
        cfReport->addColumn("Sensitivity Amount", double(), precision);
    }

    for (auto const& [tradeId, trade] : portfolio->trades()) {
        auto index = cube->idsAndIndexes().find(tradeId);
        QL_REQUIRE(index != cube->idsAndIndexes().end(), "runStressTest(): tradeId not found in cube, internal error.");
        Real npv0 = cube->getT0(index->second, 0);
        for (Size j = 0; j < scenarioGenerator->samples(); ++j) {
            const string& label = scenarioGenerator->scenarios()[j]->label();
            TLOG("Adding stress test result for trade '" << tradeId << "' and scenario #" << j << " '" << label << "'");
            Real npv = cube->get(index->second, 0, j, 0);
            Real sensi = npv - npv0;
            if (fabs(sensi) > threshold || QuantLib::close_enough(sensi, threshold)) {
                report->next();
                report->add(tradeId);
                report->add(label);
                report->add(npv0);
                report->add(npv);
                report->add(sensi);
            }
            if(cfReport) {
                for (Size k = 0; k < cfCube[index->second][0].size(); ++k) {
                    Real amount0 = cfCube[index->second][0][k].amount;
                    Real amount = Null<Real>(), amountd = Null<Real>();
                    if (cfCube[index->second][j + 1].size() > k) {
                        amount = cfCube[index->second][j + 1][k].amount;
                        amountd = amount - amount0;
                    }
                    cfReport->next();
                    cfReport->add(tradeId);
                    cfReport->add(label);
                    cfReport->add(amount0);
                    cfReport->add(amount);
                    cfReport->add(amountd);
                }
            }
        }
    }

    report->end();
    if (cfReport)
        cfReport->end();

    LOG("Stress testing done");
}

} // namespace analytics
} // namespace ore

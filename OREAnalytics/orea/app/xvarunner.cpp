/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <orea/app/xvarunner.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>
#include <orea/scenario/scenariogeneratorbuilder.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/engine/mporcalculator.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/aggregation/dimregressioncalculator.hpp>

using namespace std;
using namespace ore::data;

namespace ore {
namespace analytics {

XvaRunner::XvaRunner(Date asof,
    const string& baseCurrency,
    const boost::shared_ptr<Portfolio>& portfolio,
    const boost::shared_ptr<NettingSetManager>& netting,
    const boost::shared_ptr<EngineData>& engineData,
    const boost::shared_ptr<CurveConfigurations>& curveConfigs,
    const boost::shared_ptr<Conventions>& conventions,
    const boost::shared_ptr<TodaysMarketParameters>& todaysMarketParams,
    const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
    const boost::shared_ptr<ScenarioGeneratorData>& scenarioGeneratorData,
    const boost::shared_ptr<CrossAssetModelData>& crossAssetModelData,
    std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders,
    std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders,
    const boost::shared_ptr<ReferenceDataManager>& referenceData,
    Real dimQuantile, Size dimHorizonCalendarDays) :
    asof_(asof), baseCurrency_(baseCurrency), portfolio_(portfolio), netting_(netting), engineData_(engineData), 
    curveConfigs_(curveConfigs), conventions_(conventions), todaysMarketParams_(todaysMarketParams), simMarketData_(simMarketData), 
    scenarioGeneratorData_(scenarioGeneratorData), crossAssetModelData_(crossAssetModelData), extraLegBuilders_(extraLegBuilders),
    extraEngineBuilders_(extraEngineBuilders), dimQuantile_(dimQuantile), dimHorizonCalendarDays_(dimHorizonCalendarDays){}

void XvaRunner::runXva(const boost::shared_ptr<Market>& market, bool continueOnErr) {
    // ensure date is reset
    Settings::instance().evaluationDate() = asof_;

    CrossAssetModelBuilder modelBuilder(market, crossAssetModelData_, "", "", "", "", "", ActualActual(), false,
        continueOnErr);
    boost::shared_ptr<QuantExt::CrossAssetModel> model = *modelBuilder.model();

    ScenarioGeneratorBuilder sgb(scenarioGeneratorData_);
    boost::shared_ptr<ScenarioFactory> sf = boost::make_shared<SimpleScenarioFactory>();
    boost::shared_ptr<ScenarioGenerator> sg = sgb.build(model, sf, simMarketData_, asof_, market, "");

    boost::shared_ptr<ScenarioSimMarket> simMarket = boost::make_shared<ScenarioSimMarket>(
        market, simMarketData_, *conventions_, "", *curveConfigs_, *todaysMarketParams_, true);
    simMarket->scenarioGenerator() = sg;

    // TODO: Is this needed? PMMarket is in ORE Plus
    // Rebuild portfolio linked to SSM (wrapped)
    // auto simPmMarket = boost::make_shared<PreciousMetalMarket>(simMarket);
    boost::shared_ptr<EngineFactory> simFactory = boost::make_shared<EngineFactory>(engineData_, simMarket, 
        map<MarketContext, string>(), extraEngineBuilders_, extraLegBuilders_, referenceData_);
    portfolio_->reset();
    portfolio_->build(simFactory);

    // The cube...
    LOG("Create Cube");


    std::vector<boost::shared_ptr<ValuationCalculator>> calculators;
    boost::shared_ptr<NPVCalculator> npvCalculator = boost::make_shared<NPVCalculator>(baseCurrency_);
    boost::shared_ptr<NPVCube> cube;
    boost::shared_ptr<CubeInterpretation> cubeInterpreter;

    if (scenarioGeneratorData_->withCloseOutLag()) {
        // depth 2: NPV and close-out NPV 
        cube = boost::make_shared<SinglePrecisionInMemoryCubeN>(
            asof_, portfolio_->ids(), scenarioGeneratorData_->grid()->valuationDates(), scenarioGeneratorData_->samples(),
            2); // depth 2: default date and close-out date NPV
        cubeInterpreter = boost::make_shared<MporGridCubeInterpretation>(scenarioGeneratorData_->grid());
        // default date value stored at index 0, close-out value at index 1
        calculators.push_back(boost::make_shared<MPORCalculator>(npvCalculator, 0, 1));
    }
    else {
        // depth 2: NPV and cash flow
        cube = boost::make_shared<SinglePrecisionInMemoryCubeN>(
            asof_, portfolio_->ids(), scenarioGeneratorData_->grid()->dates(), scenarioGeneratorData_->samples(), 2);
        cubeInterpreter = boost::make_shared<RegularCubeInterpretation>();
        calculators.push_back(npvCalculator);
    }

    boost::shared_ptr<NPVCube> nettingCube = getNettingSetCube(calculators);
    
    boost::shared_ptr<AggregationScenarioData> scenarioData =
        boost::make_shared<InMemoryAggregationScenarioData>(scenarioGeneratorData_->grid()->valuationDates().size(), scenarioGeneratorData_->samples());

    simMarket->aggregationScenarioData() = scenarioData; // ??? simMarket gets agg data?

    LOG("ValEngine Build Cube");
    ValuationEngine engine(asof_, scenarioGeneratorData_->grid(), simMarket);
    engine.buildCube(portfolio_, cube, calculators, scenarioGeneratorData_->withMporStickyDate(), nettingCube);
    LOG("Got Cube");

    LOG("Run post processor");

    // FIXME, move all DIM parameters to pricer arguments
    map<string, bool> analytics;
    analytics["cva"] = true;
    analytics["dva"] = false;
    analytics["colva"] = true;
    analytics["xva"] = false;
    analytics["dim"] = true;
    analytics["cvaSensi"] = true;

    boost::shared_ptr<DynamicInitialMarginCalculator> dimCalculator = 
        getDimCalculator(cube, cubeInterpreter, scenarioData, model, nettingCube);
    postProcess_ = boost::make_shared<PostProcess>(portfolio_, netting_, market, "", cube, scenarioData, analytics, 
        baseCurrency_, "None", 1.0, 0.95, "Symmetric", "", "", "", dimCalculator, cubeInterpreter, true);

}

boost::shared_ptr<DynamicInitialMarginCalculator> XvaRunner::getDimCalculator(const boost::shared_ptr<NPVCube>& cube,
    const boost::shared_ptr<CubeInterpretation>& cubeInterpreter, const boost::shared_ptr<AggregationScenarioData>& scenarioData,
    const boost::shared_ptr<QuantExt::CrossAssetModel>& model, const boost::shared_ptr<NPVCube>& nettingCube) {

    boost::shared_ptr<DynamicInitialMarginCalculator> dimCalculator;
    Size dimRegressionOrder = 0;
    vector<string> dimRegressors; // FIXME: empty vector means regression vs netting set NPV
    Size dimLocalRegressionEvaluations = 0; // skip local regression
    Real dimLocalRegressionBandwidth = 0.25;

    dimCalculator =
        boost::make_shared<RegressionDynamicInitialMarginCalculator>(
            portfolio_, cube, cubeInterpreter, scenarioData, dimQuantile_, dimHorizonCalendarDays_, dimRegressionOrder,
            dimRegressors, dimLocalRegressionEvaluations, dimLocalRegressionBandwidth);

    return dimCalculator;
}

} // namespace analytics
} // namespace ore

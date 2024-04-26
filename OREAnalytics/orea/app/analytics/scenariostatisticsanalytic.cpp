/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <orea/app/analytics/scenariostatisticsanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/scenario/scenariowriter.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/scenario/crossassetmodelscenariogenerator.hpp>
#include <orea/scenario/scenariogeneratortransform.hpp>

#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

/******************************************************************************
 * ScenarioStatistics Analytic: Scenario_Statistics
 ******************************************************************************/

void ScenarioStatisticsAnalyticImpl::setUpConfigurations() {
    LOG("ScenarioStatisticsAnalytic::setUpConfigurations() called");
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->exposureSimMarketParams();
    analytic()->configurations().scenarioGeneratorData = inputs_->scenarioGeneratorData();
    analytic()->configurations().crossAssetModelData = inputs_->crossAssetModelData();
}

void ScenarioStatisticsAnalyticImpl::buildScenarioSimMarket() {
    
    std::string configuration = inputs_->marketConfig("simulation");
    simMarket_ = QuantLib::ext::make_shared<ScenarioSimMarket>(
            analytic()->market(),
            analytic()->configurations().simMarketParams,
            QuantLib::ext::make_shared<FixingManager>(inputs_->asof()),
            configuration,
            *inputs_->curveConfigs().get(),
            *analytic()->configurations().todaysMarketParams,
            inputs_->continueOnError(), 
            false, true, false,
            *inputs_->iborFallbackConfig(),
            false);
}

void ScenarioStatisticsAnalyticImpl::buildScenarioGenerator(const bool continueOnCalibrationError) {
    if (!model_)
        buildCrossAssetModel(continueOnCalibrationError);
    ScenarioGeneratorBuilder sgb(analytic()->configurations().scenarioGeneratorData);
    QuantLib::ext::shared_ptr<ScenarioFactory> sf = QuantLib::ext::make_shared<SimpleScenarioFactory>(true);
    string config = inputs_->marketConfig("simulation");
    scenarioGenerator_ = sgb.build(model_, sf, analytic()->configurations().simMarketParams, inputs_->asof(), analytic()->market(), config); 
    QL_REQUIRE(scenarioGenerator_, "failed to build the scenario generator"); 
    samples_ = analytic()->configurations().scenarioGeneratorData->samples();
    LOG("simulation grid size " << grid_->size());
    LOG("simulation grid valuation dates " << grid_->valuationDates().size());
    LOG("simulation grid close-out dates " << grid_->closeOutDates().size());
    LOG("simulation grid front date " << io::iso_date(grid_->dates().front()));    
    LOG("simulation grid back date " << io::iso_date(grid_->dates().back()));    

    if (inputs_->writeScenarios()) {
        auto report = QuantLib::ext::make_shared<InMemoryReport>();
        analytic()->reports()["SCENARIO_STATISTICS"]["scenario"] = report;
        scenarioGenerator_ = QuantLib::ext::make_shared<ScenarioWriter>(scenarioGenerator_, report);
    }
}

void ScenarioStatisticsAnalyticImpl::buildCrossAssetModel(const bool continueOnCalibrationError) {
    LOG("SCENARIO_STATISTICS: Build Simulation Model (continueOnCalibrationError = "
        << std::boolalpha << continueOnCalibrationError << ")");
    CrossAssetModelBuilder modelBuilder(
        analytic()->market(), analytic()->configurations().crossAssetModelData, inputs_->marketConfig("lgmcalibration"),
        inputs_->marketConfig("fxcalibration"), inputs_->marketConfig("eqcalibration"),
        inputs_->marketConfig("infcalibration"), inputs_->marketConfig("crcalibration"),
        inputs_->marketConfig("simulation"), false, continueOnCalibrationError, "",
        inputs_->salvageCorrelationMatrix() ? SalvagingAlgorithm::Spectral : SalvagingAlgorithm::None,
        "xva cam building");
    model_ = *modelBuilder.model();
}

void ScenarioStatisticsAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                  const std::set<std::string>& runTypes) {

    LOG("Scenario analytic called with asof " << io::iso_date(inputs_->asof()));

    Settings::instance().evaluationDate() = inputs_->asof();
    //ObservationMode::instance().setMode(inputs_->exposureObservationModel());

    LOG("SCENARIO_STATISTICS: Build Today's Market");
    CONSOLEW("SCENARIO_STATISTICS: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    grid_ = analytic()->configurations().scenarioGeneratorData->getGrid();

    LOG("SCENARIO_STATISTICS: Build simulation market");
    buildScenarioSimMarket();

    LOG("SCENARIO_STATISTICS: Build Scenario Generator");
    auto globalParams = inputs_->simulationPricingEngine()->globalParameters();
    auto continueOnCalErr = globalParams.find("ContinueOnCalibrationError");
    bool continueOnErr = (continueOnCalErr != globalParams.end()) && parseBool(continueOnCalErr->second);
    buildScenarioGenerator(continueOnErr);

    LOG("SCENARIO_STATISTICS: Attach Scenario Generator to ScenarioSimMarket");
    simMarket_->scenarioGenerator() = scenarioGenerator_;

    MEM_LOG;

    // Output scenario statistics and distribution reports
    const vector<RiskFactorKey>& keys = simMarket_->baseScenario()->keys();
    QuantLib::ext::shared_ptr<ScenarioGenerator> scenarioGenerator =
    inputs_->scenarioOutputZeroRate()
        ? QuantLib::ext::make_shared<ScenarioGeneratorTransform>(scenarioGenerator_, simMarket_,
                                                            analytic()->configurations().simMarketParams)
        : scenarioGenerator_;

        auto statsReport = QuantLib::ext::make_shared<InMemoryReport>();
    scenarioGenerator->reset();
        ReportWriter().writeScenarioStatistics(scenarioGenerator, keys, samples_, grid_->valuationDates(),
                                               *statsReport);
    analytic()->reports()["SCENARIO_STATISTICS"]["scenario_statistics"] = statsReport;

    auto distributionReport = QuantLib::ext::make_shared<InMemoryReport>();
    scenarioGenerator->reset();
    ReportWriter().writeScenarioDistributions(scenarioGenerator, keys, samples_, grid_->valuationDates(),
                                              inputs_->scenarioDistributionSteps(), *distributionReport);
    analytic()->reports()["SCENARIO_STATISTICS"]["scenario_distribution"] = distributionReport;
}

} // namespace analytics
} // namespace ore

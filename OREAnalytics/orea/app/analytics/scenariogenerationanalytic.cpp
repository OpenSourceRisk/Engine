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

#include <orea/app/analytics/scenariogenerationanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/scenario/clonescenariofactory.hpp>
#include <orea/scenario/crossassetmodelscenariogenerator.hpp>
#include <orea/scenario/scenariogeneratortransform.hpp>
#include <orea/scenario/scenariowriter.hpp>
#include <orea/scenario/simplescenariofactory.hpp>

#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

ScenarioGenerationAnalyticImpl::Type parseScenarioGenerationType(const string& s) {
    static map<string, ScenarioGenerationAnalyticImpl::Type> m = {
        {"stress", ScenarioGenerationAnalyticImpl::Type::stress},
        {"sensitivity", ScenarioGenerationAnalyticImpl::Type::sensitivity},
        {"exposure", ScenarioGenerationAnalyticImpl::Type::exposure}};
    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("ScenarioGenerationAnalytic Type \"" << s << "\" not recognized");
    }
}

std::ostream& operator<<(std::ostream& out, ScenarioGenerationAnalyticImpl::Type t) {
    if (t == ScenarioGenerationAnalyticImpl::Type::stress)
        out << "stress";
    else if (t == ScenarioGenerationAnalyticImpl::Type::sensitivity)
        out << "sensitivity";
    else if (t == ScenarioGenerationAnalyticImpl::Type::exposure)
        out << "exposure";
    else
        QL_FAIL("ScenarioGenerationAnalytic Type not covered");
    return out;
}

void ScenarioGenerationAnalyticImpl::setUpConfigurations() {
    LOG("ScenarioStatisticsAnalytic::setUpConfigurations() called");
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    
    string analyticStr = "scenarioGeneration";
    inputs_->loadParameter<ScenarioGenerationAnalyticImpl::Type>(
        type_, analyticStr, "scenarioType", true,
        std::function<ScenarioGenerationAnalyticImpl::Type(const string&)>(parseScenarioGenerationType));

    inputs_->loadParameterXML<ScenarioSimMarketParameters>(analytic()->configurations().simMarketParams, analyticStr,
                                                           "simulationConfigFile");
    if (type_ == ScenarioGenerationAnalyticImpl::Type::stress)
        inputs_->loadParameterXML<StressTestScenarioData>(stressTestScenarioData_, analyticStr, "stressConfigFile");
    else if (type_ == ScenarioGenerationAnalyticImpl::Type::exposure) {        
        inputs_->loadParameterXML<ScenarioGeneratorData>(analytic()->configurations().scenarioGeneratorData,
                                                         analyticStr, "simulationConfigFile");
        inputs_->loadParameterXML<CrossAssetModelData>(analytic()->configurations().crossAssetModelData, analyticStr,
                                                       "simulationConfigFile");
	}

    inputs_->loadParameter<Size>(scenarioDistributionSteps_, analyticStr, "distributionBuckets", false,
                                 std::function<Size(const string&)>(parseInteger));
    inputs_->loadParameter<bool>(scenarioOutputZeroRate_, analyticStr, "outputZeroRate", false,
                                 std::function<bool(const string&)>(parseBool));
    inputs_->loadParameter<bool>(scenarioOutputStatistics_, analyticStr, "outputStatistics", false,
                                 std::function<bool(const string&)>(parseBool));
    inputs_->loadParameter<bool>(scenarioOutputDistributions_, analyticStr, "outputDistributions", false,
                                 std::function<bool(const string&)>(parseBool));
    inputs_->loadParameter<string>(amcPathDataOutput_, analyticStr, "amcPathDataOutput", false);
}

void ScenarioGenerationAnalyticImpl::buildScenarioSimMarket() {

    std::string configuration = inputs_->marketConfig("simulation");
    simMarket_ = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), analytic()->configurations().simMarketParams,
        QuantLib::ext::make_shared<FixingManager>(inputs_->asof()), configuration, *inputs_->curveConfigs().get(),
        *analytic()->configurations().todaysMarketParams, inputs_->continueOnError(), false, true, false,
        *inputs_->iborFallbackConfig(), false);
}

void ScenarioGenerationAnalyticImpl::buildScenarioGenerator(const bool continueOnCalibrationError,
                                                            const bool allowModelFallbacks) {
    if (type_ == ScenarioGenerationAnalyticImpl::Type::exposure) {
        if (!model_)
            buildCrossAssetModel(continueOnCalibrationError, allowModelFallbacks);
        ScenarioGeneratorBuilder sgb(analytic()->configurations().scenarioGeneratorData);
        QuantLib::ext::shared_ptr<ScenarioFactory> sf = QuantLib::ext::make_shared<SimpleScenarioFactory>(true);
        string config = inputs_->marketConfig("simulation");
        scenarioGenerator_ =
            sgb.build(model_, sf, analytic()->configurations().simMarketParams, inputs_->asof(), analytic()->market(),
                      config, QuantLib::ext::make_shared<MultiPathGeneratorFactory>(), inputs_->amcPathDataOutput());
        QL_REQUIRE(scenarioGenerator_, "failed to build the scenario generator");
        samples_ = analytic()->configurations().scenarioGeneratorData->samples();
        LOG("simulation grid size " << grid_->size());
        LOG("simulation grid valuation dates " << grid_->valuationDates().size());
        LOG("simulation grid close-out dates " << grid_->closeOutDates().size());
        LOG("simulation grid front date " << io::iso_date(grid_->dates().front()));
        LOG("simulation grid back date " << io::iso_date(grid_->dates().back()));

    } else if (type_ == ScenarioGenerationAnalyticImpl::Type::stress) {
        QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket_->baseScenario();
        QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory =
            QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);
        QuantLib::ext::shared_ptr<StressScenarioGenerator> stressScenarioGenerator =
            QuantLib::ext::make_shared<StressScenarioGenerator>(
                stressTestScenarioData_, baseScenario, analytic()->configurations().simMarketParams, simMarket_,
                scenarioFactory,
                simMarket_->baseScenarioAbsolute(), false);
        scenarioGenerator_ = stressScenarioGenerator;
        samples_ = stressScenarioGenerator->samples();
    } else if (type_ == ScenarioGenerationAnalyticImpl::Type::sensitivity) {
    } else {
        QL_FAIL("ScenarioGenerationAnalyticImpl: type " << type_ << " not supported");
    }

    auto report = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    analytic()->addReport("SCENARIO_GENERATION", "scenario", report);
    scenarioGenerator_ =
        QuantLib::ext::make_shared<ScenarioWriter>(scenarioGenerator_, report, std::vector<RiskFactorKey>{}, false);
}

void ScenarioGenerationAnalyticImpl::buildCrossAssetModel(const bool continueOnCalibrationError,
                                                          const bool allowModelFallbacks) {
    LOG("XVA: Build Simulation Model (continueOnCalibrationError = "
        << std::boolalpha << continueOnCalibrationError << ", allowModelFallbacks = " << allowModelFallbacks << ")");


    CrossAssetModelBuilder modelBuilder(analytic()->market(), analytic()->configurations().crossAssetModelData,
                                        inputs_->marketConfig("lgmcalibration"), inputs_->marketConfig("fxcalibration"),
                                        inputs_->marketConfig("eqcalibration"), inputs_->marketConfig("infcalibration"),
                                        inputs_->marketConfig("crcalibration"), inputs_->marketConfig("simulation"),
                                        false, continueOnCalibrationError, "", "xva cam building", false,
                                        allowModelFallbacks);

    model_ = *modelBuilder.model();
}

void ScenarioGenerationAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                                 const std::set<std::string>& runTypes) {

    LOG("Scenario analytic called with asof " << io::iso_date(inputs_->asof()));

    Settings::instance().evaluationDate() = inputs_->asof();
    // ObservationMode::instance().setMode(inputs_->exposureObservationModel());

    LOG("SCENARIO_GENERATION: Build Today's Market");
    CONSOLEW("SCENARIO_GENERATION: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    // For exposure use the provided grid otherwise create a grid with just today
    grid_ = analytic()->configurations().scenarioGeneratorData
                ? analytic()->configurations().scenarioGeneratorData->getGrid()
                : QuantLib::ext::make_shared<DateGrid>();

    LOG("SCENARIO_GENERATION: Build simulation market");
    buildScenarioSimMarket();

    LOG("SCENARIO_GENERATION: Build Scenario Generator");
    auto continueOnErr = false;
    auto allowModelFallbacks = false;
    auto globalParams = inputs_->simulationPricingEngine()->globalParameters();
    if (auto c = globalParams.find("ContinueOnCalibrationError"); c != globalParams.end())
        continueOnErr = parseBool(c->second);
    if (auto c = globalParams.find("AllowModelFallbacks"); c != globalParams.end())
        allowModelFallbacks = parseBool(c->second);
    buildScenarioGenerator(continueOnErr, allowModelFallbacks);

    LOG("SCENARIO_GENERATION: Attach Scenario Generator to ScenarioSimMarket");
    simMarket_->scenarioGenerator() = scenarioGenerator_;

    MEM_LOG;

    // Output scenario statistics and distribution reports
    const vector<RiskFactorKey>& keys = simMarket_->baseScenario()->keys();
    QuantLib::ext::shared_ptr<ScenarioGenerator> scenarioGenerator =
        scenarioOutputZeroRate_
            ? QuantLib::ext::make_shared<ScenarioGeneratorTransform>(scenarioGenerator_, simMarket_,
                                                                     analytic()->configurations().simMarketParams)
            : scenarioGenerator_;

    if (scenarioOutputStatistics_) {
        auto statsReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
        scenarioGenerator->reset();
        ReportWriter().writeScenarioStatistics(scenarioGenerator, keys, samples_, grid_->dates(), *statsReport);
        analytic()->addReport("SCENARIO_GENERATION", "scenario_statistics", statsReport);
    }

    if (scenarioOutputDistributions_) {
        auto distributionReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
        scenarioGenerator->reset();
        ReportWriter().writeScenarioDistributions(scenarioGenerator, keys, samples_, grid_->dates(),
                                                  scenarioDistributionSteps_, *distributionReport);
        analytic()->addReport("SCENARIO_GENERATION", "scenario_distribution", distributionReport);
    }

    // if we just want to write out scenarios, loop over the samples/dates and the ScenarioWriter handles the output
    if (!(scenarioOutputDistributions_ || scenarioOutputStatistics_)) {
        const auto& dates = grid_->dates();
        for (Size i = 0; i < samples_; ++i) {
            for (Size d = 0; d < dates.size(); ++d)
                scenarioGenerator->next(dates[d]);
        }
    }
}

} // namespace analytics
} // namespace ore

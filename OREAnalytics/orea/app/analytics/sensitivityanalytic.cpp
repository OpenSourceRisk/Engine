/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <orea/app/analytics/sensitivityanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/parsensitivityanalysis.hpp>
#include <orea/engine/parsensitivitycubestream.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

void SensitivityAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().simulationConfigRequired = true;
    analytic()->configurations().sensitivityConfigRequired = true;
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->sensiSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->sensiScenarioData();
    setGenerateAdditionalResults(true);
}

void SensitivityAnalyticImpl::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                          const std::set<std::string>& runTypes) {
    if (!analytic()->match(runTypes))
        return;

    LOG("SensitivityAnalytic::runAnalytic called");

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());
    SensitivityAnalytic* sensitivityAnalytic = static_cast<SensitivityAnalytic*>(analytic());
    QL_REQUIRE(inputs_->portfolio(), "SensitivityAnalytic::run: No portfolio loaded.");

    CONSOLEW("SensitivityAnalytic: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    CONSOLEW("SensitivityAnalytic: Build Portfolio");
    analytic()->buildPortfolio();
    CONSOLE("OK");

    // This hook allows modifying the portfolio in derived classes before running the analytics below,
    // e.g. to apply SIMM exemptions.
    analytic()->modifyPortfolio();
    CONSOLEW("Risk: Sensitivity Report");

    boost::shared_ptr<InMemoryReport> report = boost::make_shared<InMemoryReport>();

    LOG("Sensi Analysis - Initialise");
    bool ccyConv = false;
    std::string configuration = inputs_->marketConfig("pricing");
    boost::shared_ptr<SensitivityAnalysis> sensiAnalysis;
    if (inputs_->nThreads() == 1) {
        LOG("Single-threaded sensi analysis");
        std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders;
        std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders;
        sensiAnalysis = boost::make_shared<SensitivityAnalysis>(
            analytic()->portfolio(), analytic()->market(), configuration, inputs_->pricingEngine(),
            analytic()->configurations().simMarketParams, analytic()->configurations().sensiScenarioData,
            inputs_->sensiRecalibrateModels(), analytic()->configurations().curveConfig,
            analytic()->configurations().todaysMarketParams, ccyConv, inputs_->refDataManager(),
            *inputs_->iborFallbackConfig(), true, inputs_->dryRun());
        LOG("Single-threaded sensi analysis created");
    } else {
        LOG("Multi-threaded sensi analysis");
        std::function<std::map<std::string, boost::shared_ptr<ore::data::AbstractTradeBuilder>>(
            const boost::shared_ptr<ReferenceDataManager>&, const boost::shared_ptr<TradeFactory>&)>
            extraTradeBuilders = {};
        std::function<std::vector<boost::shared_ptr<ore::data::EngineBuilder>>()> extraEngineBuilders = {};
        std::function<std::vector<boost::shared_ptr<ore::data::LegBuilder>>()> extraLegBuilders = {};
        sensiAnalysis = boost::make_shared<SensitivityAnalysis>(
            inputs_->nThreads(), inputs_->asof(), loader, analytic()->portfolio(), configuration,
            inputs_->pricingEngine(), analytic()->configurations().simMarketParams,
            analytic()->configurations().sensiScenarioData, inputs_->sensiRecalibrateModels(),
            analytic()->configurations().curveConfig, analytic()->configurations().todaysMarketParams, ccyConv,
            inputs_->refDataManager(), *inputs_->iborFallbackConfig(), true, inputs_->dryRun());
        LOG("Multi-threaded sensi analysis created");
    }
    // FIXME: Why are these disabled?
    set<RiskFactorKey::KeyType> typesDisabled{RiskFactorKey::KeyType::OptionletVolatility};
    boost::shared_ptr<ParSensitivityAnalysis> parAnalysis = nullptr;
    if (sensitivityAnalytic->parSensi() || sensitivityAnalytic->alignPillars()) {
        parAnalysis = boost::make_shared<ParSensitivityAnalysis>(
            inputs_->asof(), analytic()->configurations().simMarketParams,
            *analytic()->configurations().sensiScenarioData, Market::defaultConfiguration, true, typesDisabled);
        if (sensitivityAnalytic->alignPillars()) {
            LOG("Sensi analysis - align pillars (for the par conversion or because alignPillars is enabled)");
            parAnalysis->alignPillars();
            sensiAnalysis->overrideTenors(true);
        } else {
            LOG("Sensi analysis - skip aligning pillars");
        }
    }

    LOG("Sensi analysis - generate");
    sensiAnalysis->registerProgressIndicator(
        boost::make_shared<ProgressLog>("sensitivities", 100, oreSeverity::notice));
    sensiAnalysis->generateSensitivities();

    LOG("Sensi analysis - write sensitivity report in memory");
    auto baseCurrency = sensiAnalysis->simMarketData()->baseCcy();
    auto ss = boost::make_shared<SensitivityCubeStream>(sensiAnalysis->sensiCubes(), baseCurrency);
    ReportWriter(inputs_->reportNaString()).writeSensitivityReport(*report, ss, inputs_->sensiThreshold());
    analytic()->reports()[label()]["sensitivity"] = report;

    LOG("Sensi analysis - write sensitivity scenario report in memory");
    boost::shared_ptr<InMemoryReport> scenarioReport = boost::make_shared<InMemoryReport>();
    ReportWriter(inputs_->reportNaString())
        .writeScenarioReport(*scenarioReport, sensiAnalysis->sensiCubes(), inputs_->sensiThreshold());
    analytic()->reports()[label()]["sensitivity_scenario"] = scenarioReport;

    auto simmSensitivityConfigReport = boost::make_shared<InMemoryReport>();
    ReportWriter(inputs_->reportNaString())
        .writeSensitivityConfigReport(*simmSensitivityConfigReport, sensiAnalysis->scenarioGenerator()->shiftSizes(),
                                      sensiAnalysis->scenarioGenerator()->baseValues(),
                                      sensiAnalysis->scenarioGenerator()->keyToFactor());
    analytic()->reports()[label()]["sensitivity_config"] = simmSensitivityConfigReport;

    if (sensitivityAnalytic->parSensi()) {
        LOG("Sensi analysis - par conversion");

        if (sensitivityAnalytic->optimiseRiskFactors()) {
            std::set<RiskFactorKey> collectRiskFactors;
            // collect risk factors of all cubes ...
            for (auto const& c : sensiAnalysis->sensiCubes()) {
                auto currentRF = c->relevantRiskFactors();
                // ... and combine for the par analysis
                collectRiskFactors.insert(currentRF.begin(), currentRF.end());
            }
            parAnalysis->relevantRiskFactors() = collectRiskFactors;
            LOG("optimiseRiskFactors active : parSensi risk factors set to zeroSensi risk factors");
        }
        parAnalysis->computeParInstrumentSensitivities(sensiAnalysis->simMarket());
        sensitivityAnalytic->setParSensitivities(parAnalysis->parSensitivities());
        boost::shared_ptr<ParSensitivityConverter> parConverter =
            boost::make_shared<ParSensitivityConverter>(parAnalysis->parSensitivities(), parAnalysis->shiftSizes());
        auto parCube =
            boost::make_shared<ZeroToParCube>(sensiAnalysis->sensiCubes(), parConverter, typesDisabled, true);
        LOG("Sensi analysis - write par sensitivity report in memory");
        boost::shared_ptr<ParSensitivityCubeStream> pss =
            boost::make_shared<ParSensitivityCubeStream>(parCube, baseCurrency);
        // If the stream is going to be reused - wrap it into a buffered stream to gain some
        // performance. The cost for this is the memory footpring of the buffer.
        boost::shared_ptr<InMemoryReport> parSensiReport = boost::make_shared<InMemoryReport>();
        ReportWriter(inputs_->reportNaString()).writeSensitivityReport(*parSensiReport, pss, inputs_->sensiThreshold());
        analytic()->reports()[label()]["par_sensitivity"] = parSensiReport;

        if (sensitivityAnalytic->outputJacobi()) {
            boost::shared_ptr<InMemoryReport> jacobiReport = boost::make_shared<InMemoryReport>();
            writeParConversionMatrix(parAnalysis->parSensitivities(), *jacobiReport);
            analytic()->reports()[label()]["jacobi"] = jacobiReport;

            boost::shared_ptr<InMemoryReport> jacobiInverseReport = boost::make_shared<InMemoryReport>();
            parConverter->writeConversionMatrix(*jacobiInverseReport);
            analytic()->reports()[label()]["jacobi_inverse"] = jacobiInverseReport;
        }
    } else {
        LOG("Sensi Analysis - skip par conversion");
    }

    LOG("Sensi Analysis - Completed");
    CONSOLE("OK");
}

} // namespace analytics
} // namespace ore

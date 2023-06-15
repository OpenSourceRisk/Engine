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

#include <orea/app/analytics/pricinganalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/parsensitivityanalysis.hpp>
#include <orea/engine/parsensitivitycubestream.hpp>
#include <orea/engine/sensitivityanalysisplus.hpp>
#include <orea/engine/stresstest.hpp>
#include <ored/marketdata/todaysmarket.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

/*******************************************************************
 * PRICING Analytic: NPV, CASHFLOW, CASHFLOWNPV, SENSITIVITY, STRESS
 *******************************************************************/

void PricingAnalyticImpl::setUpConfigurations() {    
    if (find(begin(analytic()->analyticTypes()), end(analytic()->analyticTypes()), "SENSITIVITY") !=
        end(analytic()->analyticTypes())) {
        analytic()->configurations().simulationConfigRequired = true;
        analytic()->configurations().sensitivityConfigRequired = true;
    }  

    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->sensiSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->sensiScenarioData();

    setGenerateAdditionalResults(true);
}

void PricingAnalyticImpl::runAnalytic( 
    const boost::shared_ptr<ore::data::InMemoryLoader>& loader, 
    const std::set<std::string>& runTypes) {

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());

    QL_REQUIRE(inputs_->portfolio(), "PricingAnalytic::run: No portfolio loaded.");

    CONSOLEW("Pricing: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    CONSOLEW("Pricing: Build Portfolio");
    analytic()->buildPortfolio();
    CONSOLE("OK");

    // Check coverage
    for (const auto& rt : runTypes) {
        if (std::find(analytic()->analyticTypes().begin(), analytic()->analyticTypes().end(), rt) ==
            analytic()->analyticTypes().end()) {
            DLOG("requested analytic " << rt << " not covered by the PricingAnalytic");
        }
    }

    // This hook allows modifying the portfolio in derived classes before running the analytics below,
    // e.g. to apply SIMM exemptions.
    analytic()->modifyPortfolio();

    for (const auto& type : analytic()->analyticTypes()) {
        boost::shared_ptr<InMemoryReport> report = boost::make_shared<InMemoryReport>();
        InMemoryReport tmpReport;
        // skip analytics not requested
        if (runTypes.find(type) == runTypes.end())
            continue;

        std::string effectiveResultCurrency =
            inputs_->resultCurrency().empty() ? inputs_->baseCurrency() : inputs_->resultCurrency();
        if (type == "NPV") {
            CONSOLEW("Pricing: NPV Report");
            ReportWriter(inputs_->reportNaString())
                .writeNpv(*report, effectiveResultCurrency, analytic()->market(), "",
                          analytic()->portfolio());
            analytic()->reports()[type]["npv"] = report;
            CONSOLE("OK");
            if (inputs_->outputAdditionalResults()) {
                CONSOLEW("Pricing: Additional Results");
                boost::shared_ptr<InMemoryReport> addReport = boost::make_shared<InMemoryReport>();;
                ReportWriter(inputs_->reportNaString())
                    .writeAdditionalResultsReport(*addReport, analytic()->portfolio(), analytic()->market(),
                                                  effectiveResultCurrency);
                analytic()->reports()[type]["additional_results"] = addReport;
                CONSOLE("OK");
            }
            // FIXME: Leave this here as additional output within the NPV analytic, or store report as separate analytic?
            if (inputs_->outputTodaysMarketCalibration()) {
                CONSOLEW("Pricing: Market Calibration");
                LOG("Write todays market calibration report");
                auto t = boost::dynamic_pointer_cast<TodaysMarket>(analytic()->market());
                QL_REQUIRE(t != nullptr, "expected todays market instance");
                boost::shared_ptr<InMemoryReport> mktReport = boost::make_shared<InMemoryReport>();
                ReportWriter(inputs_->reportNaString())
                    .writeTodaysMarketCalibrationReport(*mktReport, t->calibrationInfo());
                analytic()->reports()[type]["todaysmarketcalibration"] = mktReport;
                CONSOLE("OK");
            }
            if (inputs_->outputCurves()) {
                CONSOLEW("Pricing: Curves Report");
                LOG("Write curves report");
                boost::shared_ptr<InMemoryReport> curvesReport = boost::make_shared<InMemoryReport>();
                DateGrid grid(inputs_->curvesGrid());
                std::string config = inputs_->curvesMarketConfig();
                ReportWriter(inputs_->reportNaString())
                    .writeCurves(*curvesReport, config, grid, *inputs_->todaysMarketParams(),
                                 analytic()->market(), inputs_->continueOnError());
                analytic()->reports()[type]["curves"] = curvesReport;
                CONSOLE("OK");
            }
        }
        else if (type == "CASHFLOW") {
            CONSOLEW("Pricing: Cashflow Report");
            string marketConfig = inputs_->marketConfig("pricing");
            ReportWriter(inputs_->reportNaString())
                .writeCashflow(*report, effectiveResultCurrency, analytic()->portfolio(),
                               analytic()->market(),
                               marketConfig, inputs_->includePastCashflows());
            analytic()->reports()[type]["cashflow"] = report;
            CONSOLE("OK");
        }
        else if (type == "CASHFLOWNPV") {
            CONSOLEW("Pricing: Cashflow NPV report");
            string marketConfig = inputs_->marketConfig("pricing");
            ReportWriter(inputs_->reportNaString())
                .writeCashflow(tmpReport, effectiveResultCurrency, analytic()->portfolio(),
                               analytic()->market(),
                               marketConfig, inputs_->includePastCashflows());
            ReportWriter(inputs_->reportNaString())
                .writeCashflowNpv(*report, tmpReport, analytic()->market(), marketConfig,
                                  effectiveResultCurrency, inputs_->cashflowHorizon());
            analytic()->reports()[type]["cashflownpv"] = report;
            CONSOLE("OK");
        }
        else if (type == "STRESS") {
            CONSOLEW("Risk: Stress Test Report");
            LOG("Stress Test Analysis called");
            Settings::instance().evaluationDate() = inputs_->asof();
            std::string marketConfig = inputs_->marketConfig("pricing");
            std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders;
            std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders;
            boost::shared_ptr<StressTest> stressTest = boost::make_shared<StressTest>(
                analytic()->portfolio(), analytic()->market(), marketConfig, inputs_->pricingEngine(),
                inputs_->stressSimMarketParams(), inputs_->stressScenarioData(),
                *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams, nullptr,
                inputs_->refDataManager(), *inputs_->iborFallbackConfig(),
                inputs_->continueOnError());
            stressTest->writeReport(report, inputs_->stressThreshold());
            analytic()->reports()[type]["stress"] = report;
            CONSOLE("OK");
        }
        else if (type == "SENSITIVITY") {
            CONSOLEW("Risk: Sensitivity Report");
            LOG("Sensi Analysis - Initialise");
            bool recalibrateModels = true;
            bool ccyConv = false;
            std::string configuration = inputs_->marketConfig("pricing");
            boost::shared_ptr<SensitivityAnalysis> sensiAnalysis;
            if (inputs_->nThreads() == 1) {
                LOG("Single-threaded sensi analysis");
                std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraEngineBuilders;
                std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders;
                sensiAnalysis = boost::make_shared<SensitivityAnalysisPlus>(
                    analytic()->portfolio(), analytic()->market(), configuration, inputs_->pricingEngine(),
                    analytic()->configurations().simMarketParams, analytic()->configurations().sensiScenarioData,
                    recalibrateModels, analytic()->configurations().curveConfig,
                    analytic()->configurations().todaysMarketParams, ccyConv, inputs_->refDataManager(),
                    *inputs_->iborFallbackConfig(), true, inputs_->dryRun());
                LOG("Single-threaded sensi analysis created");
            }
            else {
                LOG("Multi-threaded sensi analysis");
                std::function<std::map<std::string, boost::shared_ptr<ore::data::AbstractTradeBuilder>>(
                    const boost::shared_ptr<ReferenceDataManager>&, const boost::shared_ptr<TradeFactory>&)>
                    extraTradeBuilders = {};
                std::function<std::vector<boost::shared_ptr<ore::data::EngineBuilder>>()> extraEngineBuilders = {};
                std::function<std::vector<boost::shared_ptr<ore::data::LegBuilder>>()> extraLegBuilders = {};
                sensiAnalysis = boost::make_shared<SensitivityAnalysisPlus>(
                    inputs_->nThreads(), inputs_->asof(), loader, analytic()->portfolio(),
                    Market::defaultConfiguration, inputs_->pricingEngine(),
                    analytic()->configurations().simMarketParams, analytic()->configurations().sensiScenarioData, 
                    recalibrateModels, analytic()->configurations().curveConfig,
                    analytic()->configurations().todaysMarketParams, ccyConv, inputs_->refDataManager(),
                    *inputs_->iborFallbackConfig(), true, inputs_->dryRun());
                LOG("Multi-threaded sensi analysis created");
            }
            // FIXME: Why are these disabled?
            set<RiskFactorKey::KeyType> typesDisabled{RiskFactorKey::KeyType::OptionletVolatility};
            boost::shared_ptr<ParSensitivityAnalysis> parAnalysis = nullptr;
            if (inputs_->parSensi() || inputs_->alignPillars()) {
                parAnalysis= boost::make_shared<ParSensitivityAnalysis>(
                    inputs_->asof(), analytic()->configurations().simMarketParams,
                    *analytic()->configurations().sensiScenarioData, "",
                    true, typesDisabled);
                if (inputs_->alignPillars()) {
                    LOG("Sensi analysis - align pillars (for the par conversion or because alignPillars is enabled)");
                    parAnalysis->alignPillars();
                    sensiAnalysis->overrideTenors(true);
                } else {
                    LOG("Sensi analysis - skip aligning pillars");
                }
            }

            LOG("Sensi analysis - generate");
            sensiAnalysis->registerProgressIndicator(boost::make_shared<ProgressLog>("sensitivities", 100, ORE_NOTICE));
            sensiAnalysis->generateSensitivities();

            LOG("Sensi analysis - write sensitivity report in memory");
            auto baseCurrency = sensiAnalysis->simMarketData()->baseCcy();
            auto ss = boost::make_shared<SensitivityCubeStream>(sensiAnalysis->sensiCube(), baseCurrency);
            ReportWriter(inputs_->reportNaString())
                .writeSensitivityReport(*report, ss, inputs_->sensiThreshold());
            analytic()->reports()[type]["sensitivity"] = report;

            LOG("Sensi analysis - write sensitivity scenario report in memory");
            boost::shared_ptr<InMemoryReport> scenarioReport = boost::make_shared<InMemoryReport>();
            ReportWriter(inputs_->reportNaString())
                .writeScenarioReport(*scenarioReport, sensiAnalysis->sensiCube(),
                                     inputs_->sensiThreshold());
            analytic()->reports()[type]["sensitivity_scenario"] = scenarioReport;

            if (inputs_->parSensi()) {
                LOG("Sensi analysis - par conversion");
                parAnalysis->computeParInstrumentSensitivities(sensiAnalysis->simMarket());
                boost::shared_ptr<ParSensitivityConverter> parConverter =
                    boost::make_shared<ParSensitivityConverter>(parAnalysis->parSensitivities(), parAnalysis->shiftSizes());
                auto parCube = boost::make_shared<ZeroToParCube>(sensiAnalysis->sensiCube(), parConverter, typesDisabled, true);
                LOG("Sensi analysis - write par sensitivity report in memory");
                boost::shared_ptr<ParSensitivityCubeStream> pss = boost::make_shared<ParSensitivityCubeStream>(parCube, baseCurrency);
                // If the stream is going to be reused - wrap it into a buffered stream to gain some
                // performance. The cost for this is the memory footpring of the buffer.
                boost::shared_ptr<InMemoryReport> parSensiReport = boost::make_shared<InMemoryReport>();
                ReportWriter(inputs_->reportNaString())
                    .writeSensitivityReport(*parSensiReport, pss, inputs_->sensiThreshold());
                analytic()->reports()[type]["par_sensitivity"] = parSensiReport;

                if (inputs_->outputJacobi()) {
                    boost::shared_ptr<InMemoryReport> jacobiReport = boost::make_shared<InMemoryReport>();
                    writeParConversionMatrix(parAnalysis->parSensitivities(), *jacobiReport);
                    analytic()->reports()[type]["jacobi"] = jacobiReport;
                    
                    boost::shared_ptr<InMemoryReport> jacobiInverseReport = boost::make_shared<InMemoryReport>();
                    parConverter->writeConversionMatrix(*jacobiInverseReport);
                    analytic()->reports()[type]["jacobi_inverse"] = jacobiInverseReport;
                }
            }
            else {
                LOG("Sensi Analysis - skip par conversion");
            }
        
            LOG("Sensi Analysis - Completed");
            CONSOLE("OK");
        }
        else {
            QL_FAIL("PricingAnalytic type " << type << " invalid");
        }
    }
}

} // namespace analytics
} // namespace ore

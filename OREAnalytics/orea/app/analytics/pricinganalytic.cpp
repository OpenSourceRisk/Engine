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
    const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader, 
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
        QuantLib::ext::shared_ptr<InMemoryReport> report = QuantLib::ext::make_shared<InMemoryReport>();
        InMemoryReport tmpReport;
        // skip analytics not requested
        if (runTypes.find(type) == runTypes.end())
            continue;

        std::string effectiveResultCurrency =
            inputs_->resultCurrency().empty() ? inputs_->baseCurrency() : inputs_->resultCurrency();
        if (type == "NPV") {
            CONSOLEW("Pricing: NPV Report");
            ReportWriter(inputs_->reportNaString())
                .writeNpv(*report, effectiveResultCurrency, analytic()->market(), inputs_->marketConfig("pricing"),
                          analytic()->portfolio());
            analytic()->reports()[type]["npv"] = report;
            CONSOLE("OK");
            if (inputs_->outputAdditionalResults()) {
                CONSOLEW("Pricing: Additional Results");
                QuantLib::ext::shared_ptr<InMemoryReport> addReport = QuantLib::ext::make_shared<InMemoryReport>();;
                ReportWriter(inputs_->reportNaString())
                    .writeAdditionalResultsReport(*addReport, analytic()->portfolio(), analytic()->market(),
                                                  effectiveResultCurrency, inputs_->additionalResultsReportPrecision());
                analytic()->reports()[type]["additional_results"] = addReport;
                CONSOLE("OK");
            }
            if (inputs_->outputCurves()) {
                CONSOLEW("Pricing: Curves Report");
                LOG("Write curves report");
                QuantLib::ext::shared_ptr<InMemoryReport> curvesReport = QuantLib::ext::make_shared<InMemoryReport>();
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
        else if (type == "SENSITIVITY") {
            CONSOLEW("Risk: Sensitivity Report");
            LOG("Sensi Analysis - Initialise");
            bool recalibrateModels = true;
            bool ccyConv = false;
            std::string configuration = inputs_->marketConfig("pricing");
            QuantLib::ext::shared_ptr<SensitivityAnalysis> sensiAnalysis;
            if (inputs_->nThreads() == 1) {
                LOG("Single-threaded sensi analysis");
                sensiAnalysis = QuantLib::ext::make_shared<SensitivityAnalysis>(
                    analytic()->portfolio(), analytic()->market(), configuration, inputs_->pricingEngine(),
                    analytic()->configurations().simMarketParams, analytic()->configurations().sensiScenarioData,
                    recalibrateModels, analytic()->configurations().curveConfig,
                    analytic()->configurations().todaysMarketParams, ccyConv, inputs_->refDataManager(),
                    *inputs_->iborFallbackConfig(), true, inputs_->dryRun());
                LOG("Single-threaded sensi analysis created");
            }
            else {
                LOG("Multi-threaded sensi analysis");
                sensiAnalysis = QuantLib::ext::make_shared<SensitivityAnalysis>(
                    inputs_->nThreads(), inputs_->asof(), loader, analytic()->portfolio(), configuration,
                    inputs_->pricingEngine(), analytic()->configurations().simMarketParams,
                    analytic()->configurations().sensiScenarioData, recalibrateModels,
                    analytic()->configurations().curveConfig, analytic()->configurations().todaysMarketParams, ccyConv,
                    inputs_->refDataManager(), *inputs_->iborFallbackConfig(), true, inputs_->dryRun());
                LOG("Multi-threaded sensi analysis created");
            }
            // FIXME: Why are these disabled?
            set<RiskFactorKey::KeyType> typesDisabled{RiskFactorKey::KeyType::OptionletVolatility};
            QuantLib::ext::shared_ptr<ParSensitivityAnalysis> parAnalysis = nullptr;
            if (inputs_->parSensi() || inputs_->alignPillars()) {
                parAnalysis= QuantLib::ext::make_shared<ParSensitivityAnalysis>(
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
            sensiAnalysis->registerProgressIndicator(QuantLib::ext::make_shared<ProgressLog>("sensitivities", 100, oreSeverity::notice));
            sensiAnalysis->generateSensitivities();

            LOG("Sensi analysis - write sensitivity report in memory");
            auto baseCurrency = sensiAnalysis->simMarketData()->baseCcy();
            auto ss = QuantLib::ext::make_shared<SensitivityCubeStream>(sensiAnalysis->sensiCubes(), baseCurrency);
            ReportWriter(inputs_->reportNaString())
                .writeSensitivityReport(*report, ss, inputs_->sensiThreshold());
            analytic()->reports()[type]["sensitivity"] = report;

            LOG("Sensi analysis - write sensitivity scenario report in memory");
            QuantLib::ext::shared_ptr<InMemoryReport> scenarioReport = QuantLib::ext::make_shared<InMemoryReport>();
            ReportWriter(inputs_->reportNaString())
                .writeScenarioReport(*scenarioReport, sensiAnalysis->sensiCubes(),
                                     inputs_->sensiThreshold());
            analytic()->reports()[type]["sensitivity_scenario"] = scenarioReport;

            auto simmSensitivityConfigReport = QuantLib::ext::make_shared<InMemoryReport>();
            ReportWriter(inputs_->reportNaString())
                .writeSensitivityConfigReport(*simmSensitivityConfigReport,
                                              sensiAnalysis->scenarioGenerator()->shiftSizes(),
                                              sensiAnalysis->scenarioGenerator()->baseValues(),
                                              sensiAnalysis->scenarioGenerator()->keyToFactor());
            analytic()->reports()[type]["sensitivity_config"] = simmSensitivityConfigReport;

            if (inputs_->parSensi()) {
                LOG("Sensi analysis - par conversion");

                if (inputs_->optimiseRiskFactors()){
                    std::set<RiskFactorKey> collectRiskFactors;
                    // collect risk factors of all cubes ...
                    for(auto const& c : sensiAnalysis->sensiCubes()){
                        auto currentRF = c->relevantRiskFactors();
                        // ... and combine for the par analysis
                        collectRiskFactors.insert(currentRF.begin(), currentRF.end());
                    }
                    parAnalysis->relevantRiskFactors() = collectRiskFactors;
                    LOG("optimiseRiskFactors active : parSensi risk factors set to zeroSensi risk factors");
                }
                parAnalysis->computeParInstrumentSensitivities(sensiAnalysis->simMarket());
                QuantLib::ext::shared_ptr<ParSensitivityConverter> parConverter =
                    QuantLib::ext::make_shared<ParSensitivityConverter>(parAnalysis->parSensitivities(), parAnalysis->shiftSizes());
                auto parCube = QuantLib::ext::make_shared<ZeroToParCube>(sensiAnalysis->sensiCubes(), parConverter, typesDisabled, true);
                LOG("Sensi analysis - write par sensitivity report in memory");
                QuantLib::ext::shared_ptr<ParSensitivityCubeStream> pss =
                    QuantLib::ext::make_shared<ParSensitivityCubeStream>(parCube, baseCurrency);
                // If the stream is going to be reused - wrap it into a buffered stream to gain some
                // performance. The cost for this is the memory footpring of the buffer.
                QuantLib::ext::shared_ptr<InMemoryReport> parSensiReport = QuantLib::ext::make_shared<InMemoryReport>();
                ReportWriter(inputs_->reportNaString())
                    .writeSensitivityReport(*parSensiReport, pss, inputs_->sensiThreshold());
                analytic()->reports()[type]["par_sensitivity"] = parSensiReport;

                if (inputs_->outputJacobi()) {
                    QuantLib::ext::shared_ptr<InMemoryReport> jacobiReport = QuantLib::ext::make_shared<InMemoryReport>();
                    writeParConversionMatrix(parAnalysis->parSensitivities(), *jacobiReport);
                    analytic()->reports()[type]["jacobi"] = jacobiReport;
                    
                    QuantLib::ext::shared_ptr<InMemoryReport> jacobiInverseReport = QuantLib::ext::make_shared<InMemoryReport>();
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

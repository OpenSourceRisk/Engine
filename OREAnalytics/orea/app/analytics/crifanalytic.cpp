/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <orea/app/analytics/crifanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/simm/crifgenerator.hpp>
#include <orea/simm/crifmarket.hpp>
#include <orea/simm/portfoliomodifier.hpp>
#include <orea/simm/simmtradedata.hpp>
#include <orea/simm/utilities.hpp>
#include <orea/engine/bufferedsensitivitystream.hpp>
#include <orea/engine/decomposedsensitivitystream.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/parsensitivitycubestream.hpp>
#include <orea/engine/zerotoparcube.hpp>

#include <ored/marketdata/todaysmarket.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/parsers.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

std::pair<QuantLib::ext::shared_ptr<SensitivityStream>,
          std::map<std::string, QuantLib::ext::shared_ptr<InMemoryReport>>>
computeSensitivities(QuantLib::ext::shared_ptr<ore::analytics::SensitivityAnalysis>& sensiAnalysis,
                     const QuantLib::ext::shared_ptr<InputParameters>& inputs, ore::analytics::Analytic* analytic,
                     const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const bool writeReports) {
    
    analytic->startTimer("computeSensitivities()");
    
    LOG("Initialise sensitivity analysis");

    map<string, QuantLib::ext::shared_ptr<InMemoryReport>> sensiReports;
    if (inputs->nThreads() == 1) {
        sensiAnalysis = QuantLib::ext::make_shared<SensitivityAnalysis>(
            portfolio, analytic->market(), Market::defaultConfiguration, inputs->pricingEngine(),
            analytic->configurations().simMarketParams, analytic->configurations().sensiScenarioData,
            inputs->sensiRecalibrateModels(), inputs->sensiLaxFxConversion(),
            analytic->configurations().curveConfig, analytic->configurations().todaysMarketParams, false,
            inputs->refDataManager(), *inputs->iborFallbackConfig(), true, inputs->dryRun());
    } else {
        sensiAnalysis = QuantLib::ext::make_shared<SensitivityAnalysis>(
            inputs->nThreads(), inputs->asof(), analytic->loader(), portfolio, Market::defaultConfiguration,
            inputs->pricingEngine(), analytic->configurations().simMarketParams,
            analytic->configurations().sensiScenarioData, inputs->sensiRecalibrateModels(),
            inputs->sensiLaxFxConversion(), analytic->configurations().curveConfig,
            analytic->configurations().todaysMarketParams, false, inputs->refDataManager(),
            *inputs->iborFallbackConfig(), true, inputs->dryRun(), "analytic/" + analytic->label());
    }

    LOG("Sensitivity analysis initialised");
    MEM_LOG;

    LOG("Align pillars for the par sensitivity calculation");
    set<RiskFactorKey::KeyType> typesDisabled{RiskFactorKey::KeyType::OptionletVolatility};
    QuantLib::ext::shared_ptr<ParSensitivityAnalysis> parAnalysis = QuantLib::ext::make_shared<ParSensitivityAnalysis>(
        inputs->asof(), analytic->configurations().simMarketParams, *analytic->configurations().sensiScenarioData,
        Market::defaultConfiguration, true, typesDisabled);
    parAnalysis->alignPillars();
    sensiAnalysis->overrideTenors(true);
    LOG("Pillars aligned");
    MEM_LOG;

    LOG("Generate sensitivities");
    sensiAnalysis->registerProgressIndicator(QuantLib::ext::make_shared<ProgressLog>("sensi sim"));
    sensiAnalysis->generateSensitivities();
    LOG("Sensitivities generated");
    MEM_LOG;

    ReportWriter reportWriter(inputs->reportNaString());

    if (writeReports) {
        auto simmScenarioReport = QuantLib::ext::make_shared<InMemoryReport>();
        reportWriter.writeScenarioReport(*simmScenarioReport, sensiAnalysis->sensiCubes(),
                                         inputs->sensiThreshold());
        sensiReports["simm_scenario"] = simmScenarioReport;
    } else {
        LOG("Skipping SIMM scenario report, this is an optional report and writeOptionalReports is set to false");
    }
    MEM_LOG;

    QuantLib::ext::shared_ptr<SensitivityStream> ss = QuantLib::ext::make_shared<SensitivityCubeStream>(
        sensiAnalysis->sensiCubes(), analytic->configurations().simMarketParams->baseCcy());
    if (writeReports) {
        auto simmSensitivityReport = QuantLib::ext::make_shared<InMemoryReport>();
        reportWriter.writeSensitivityReport(*simmSensitivityReport, ss, inputs->sensiThreshold());
        sensiReports["simm_sensitivity"] = simmSensitivityReport;
    } else {
        LOG("Skipping SIMM sensitivity report, this is an optional report and writeOptionalReports is set to "
            "false");
    }
    MEM_LOG;

    if (writeReports) {
        auto simmSensitivityConfigReport = QuantLib::ext::make_shared<InMemoryReport>();
        reportWriter.writeSensitivityConfigReport(
            *simmSensitivityConfigReport, sensiAnalysis->scenarioGenerator()->shiftSizes(),
            sensiAnalysis->scenarioGenerator()->baseValues(), sensiAnalysis->scenarioGenerator()->keyToFactor());
        sensiReports["simm_sensitivity_config"] = simmSensitivityConfigReport;
    } else {
        LOG("Skipping SIMM sensitivity config report, this is an optional report and writeOptionalReports is set "
            "to false");
    }
    MEM_LOG;

    parAnalysis->computeParInstrumentSensitivities(sensiAnalysis->simMarket());
    QuantLib::ext::shared_ptr<ParSensitivityConverter> parConverter =
        QuantLib::ext::make_shared<ParSensitivityConverter>(parAnalysis->parSensitivities(), parAnalysis->shiftSizes());
    auto parCube = QuantLib::ext::make_shared<ZeroToParCube>(sensiAnalysis->sensiCubes(), parConverter, typesDisabled, true);
    ss = QuantLib::ext::make_shared<ParSensitivityCubeStream>(parCube, analytic->configurations().simMarketParams->baseCcy());
    // The stream will be reused for the crif generation, so we wrap it into a buffered stream to gain some
    // performance. The cost for this is the memory footpring of the buffer.
    ss = QuantLib::ext::make_shared<ore::analytics::BufferedSensitivityStream>(ss);
    if (writeReports) {
        auto simmParSensitivityReport = QuantLib::ext::make_shared<InMemoryReport>();
        reportWriter.writeSensitivityReport(*simmParSensitivityReport, ss, inputs->sensiThreshold());
        sensiReports["simm_par_sensitivity"] = simmParSensitivityReport;
    }
    MEM_LOG;

    if (writeReports && inputs->outputJacobi()) {
        auto jacobiReport = QuantLib::ext::make_shared<InMemoryReport>();
        writeParConversionMatrix(parAnalysis->parSensitivities(), *jacobiReport);
        sensiReports["simm_par_conversion_matrix"] = jacobiReport;

        auto jacobiInverseReport = QuantLib::ext::make_shared<InMemoryReport>();
        parConverter->writeConversionMatrix(*jacobiInverseReport);
        sensiReports["simm_par_conversion_matrix_inverse"] = jacobiInverseReport;
    }

    analytic->stopTimer("computeSensitivities()");

    return std::make_pair(ss, sensiReports);
};
  
void CrifAnalyticImpl::setUpConfigurations() {
    LOG("CrifAnalytic::setUpConfigurations() called");
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->sensiSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->sensiScenarioData();

    QL_REQUIRE(analytic()->configurations().simMarketParams, "CrifAnalytic: simMarketParams not set");
    QL_REQUIRE(analytic()->configurations().sensiScenarioData, "CrifAnalytic: sensiScenarioData not set");
    QL_REQUIRE(analytic()->configurations().todaysMarketParams, "CrifAnalytic: todaysMarketParams not set");
    
    setGenerateAdditionalResults(true);
}

void CrifAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                   const std::set<std::string>& runTypes) {
    QL_REQUIRE(analytic()->portfolio() || inputs_->portfolio(), "CrifAnalytic::run: No portfolio loaded.");

    CONSOLEW("CRIF: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    CONSOLEW("CRIF: Build Portfolio");
    analytic()->buildPortfolio();
    CONSOLE("OK");

    analytic()->enrichIndexFixings(analytic()->portfolio());

    ObservationMode::instance().setMode(ObservationMode::Mode::None);

    auto crifAnalytic = static_cast<CrifAnalytic*>(analytic());
    QL_REQUIRE(crifAnalytic, "Analytic must be of type CRIF");

    // Save portfolio state before applying SIMM exemptions
    auto portfolioNoSimmExemptions = QuantLib::ext::make_shared<Portfolio>();
    for (const auto& t : analytic()->portfolio()->trades())
        portfolioNoSimmExemptions->add(t.second);
    crifAnalytic->setPortfolioNoSimmExemptions(portfolioNoSimmExemptions);

    auto marketConfig = inputs_->marketConfig("pricing");

    // NPV report before applying SIMM exemptions
    auto npvWithoutReport = QuantLib::ext::make_shared<InMemoryReport>();
    ReportWriter(inputs_->reportNaString())
        .writeNpv(*npvWithoutReport, crifAnalytic->baseCurrency(), analytic()->market(), marketConfig,
                  analytic()->portfolio());
    analytic()->addReport(LABEL, "npv_no_simm_exemptions", npvWithoutReport);

    std::set<std::string> removedTrades, modifiedTrades;
    analytic()->startTimer("applySimmExemptions()");
    try {
        std::tie(removedTrades, modifiedTrades) =
            applySimmExemptions(*analytic()->portfolio(), engineFactory(), crifAnalytic->simmExemptionOverrides());
    } catch (std::exception& e) {
        QL_FAIL(e.what());
    }
    analytic()->stopTimer("applySimmExemptions()");

    // If we have an empty portfolio, then quit the CRIF analytic
    if (analytic()->portfolio()->size() == 0) {
        ALOG("portfolio is empty once SIMM exemptions applied");
        analytic()->addReport(LABEL, "crif", QuantLib::ext::make_shared<InMemoryReport>());
        return;
    }

    // Save portfolio state after applying SIMM exemptions
    auto portfolioSimmExemptions = QuantLib::ext::make_shared<Portfolio>();
    for (const auto& t : analytic()->portfolio()->trades())
        portfolioSimmExemptions->add(t.second);
    crifAnalytic->setPortfolioSimmExemptions(portfolioSimmExemptions);

    // NPV report after applying SIMM exemptions
    auto npvWithReport = QuantLib::ext::make_shared<InMemoryReport>();
    ReportWriter(inputs_->reportNaString())
        .writeNpv(*npvWithReport, crifAnalytic->baseCurrency(), analytic()->market(), marketConfig,
                  analytic()->portfolio());
    analytic()->addReport(LABEL, "npv_with_simm_exemptions", npvWithReport);

    // CF report after applying SIMM exemptions
    auto cfWithReport = QuantLib::ext::make_shared<InMemoryReport>();
    ReportWriter(inputs_->reportNaString())
        .writeCashflow(*cfWithReport, crifAnalytic->baseCurrency(), analytic()->portfolio(), analytic()->market(),
                       marketConfig);
    analytic()->addReport(LABEL, "cashflow_with_simm_exemptions", cfWithReport);

    // Portfolio after applying SIMM exemptions
    LOG("Write portfolio, with SIMM exemptions, to XML");
    path portfolioXmlPath = inputs_->resultsPath() / "portfolio_with_simm_exemptions.xml";
    crifAnalytic->portfolioSimmExemptions()->toFile(portfolioXmlPath.string());

    // Compute sensitivities for the portfolio and write additional reports
    QuantLib::ext::shared_ptr<SensitivityStream> ss;
    map<string, QuantLib::ext::shared_ptr<InMemoryReport>> sensiReports;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityAnalysis> sensiAnalysis;
    if (portfolioSimmExemptions->size() > 0) {
        LOG("Begin sensitivity and par sensitivity analysis");
	CONSOLEW("CRIF: Run Sensitivity");
        auto res = computeSensitivities(sensiAnalysis, inputs_, analytic(), portfolioSimmExemptions, true);
        ss = res.first;
        sensiReports = res.second;
        LOG("Finished sensitivity and par sensitivity analysis");

        // Write out reports from computed sensitivities (for portfolio only)
        if (sensiReports.find("simm_scenario") != sensiReports.end()) {
            if (analytic()->getWriteIntermediateReports()) {
                LOG("Write SIMM scenario report");
                path simmScenarioPath = inputs_->resultsPath() / "simm_scenario.csv";
                sensiReports.at("simm_scenario")
                    ->toFile(simmScenarioPath.string(), ',', false, inputs_->csvQuoteChar(),
                             inputs_->reportNaString());
                LOG("Scenario report written to " << simmScenarioPath);
            } else
                analytic()->addReport(LABEL, "simm_scenario", sensiReports.at("simm_scenario"));
        }
        if (sensiReports.find("simm_sensitivity") != sensiReports.end()) {
            if (analytic()->getWriteIntermediateReports()) {
                LOG("Write SIMM sensitivity report");
                path simmSensitivityPath = inputs_->resultsPath() / "simm_sensitivity.csv";
                sensiReports.at("simm_sensitivity")
                    ->toFile(simmSensitivityPath.string(), ',', false, inputs_->csvQuoteChar(),
                             inputs_->reportNaString());
                LOG("Sensitivity report written to " << simmSensitivityPath);
            } else
                analytic()->addReport(LABEL, "simm_sensitivity", sensiReports.at("simm_sensitivity"));
        }
        if (sensiReports.find("simm_sensitivity_config") != sensiReports.end()) {
            if (analytic()->getWriteIntermediateReports()) {
                LOG("Write SIMM sensitivity config report");
                path simmSensitivityConfigPath = inputs_->resultsPath() / "simm_sensitivity_config.csv";
                sensiReports.at("simm_sensitivity_config")
                    ->toFile(simmSensitivityConfigPath.string(), ',', false, inputs_->csvQuoteChar(),
                             inputs_->reportNaString());
                LOG("Sensitivity config report written to " << simmSensitivityConfigPath);
            } else
                analytic()->addReport(LABEL, "simm_sensitivity_config", sensiReports.at("simm_sensitivity_config"));
        }
        if (sensiReports.find("simm_par_sensitivity") != sensiReports.end()) {
            if (analytic()->getWriteIntermediateReports()) {
                LOG("Write SIMM par sensitivity report");
                path simmParSensitivityPath = inputs_->resultsPath() / "simm_par_sensitivity.csv";
                sensiReports.at("simm_par_sensitivity")
                    ->toFile(simmParSensitivityPath.string(), ',', false, inputs_->csvQuoteChar(),
                             inputs_->reportNaString());
                LOG("SIMM par sensitivity report written to " << simmParSensitivityPath);
            } else
                analytic()->addReport(LABEL, "simm_par_sensitivity", sensiReports.at("simm_par_sensitivity"));
        }

        if (sensiReports.find("simm_par_conversion_matrix") != sensiReports.end() &&
            analytic()->getWriteIntermediateReports()) {
            path simmParJacobiPath = inputs_->resultsPath() / "simm_par_conversion_matrix.csv";
            sensiReports.at("simm_par_conversion_matrix")
                ->toFile(simmParJacobiPath.string(), ',', false, inputs_->csvQuoteChar(),
                         inputs_->reportNaString());
        }
        if (sensiReports.find("simm_par_conversion_matrix_inverse") != sensiReports.end() &&
            analytic()->getWriteIntermediateReports()) {
            path simmParJacobiInversePath = inputs_->resultsPath() / "simm_par_conversion_matrix_inverse.csv";
            sensiReports.at("simm_par_conversion_matrix_inverse")
                ->toFile(simmParJacobiInversePath.string(), ',', false, inputs_->csvQuoteChar(),
                         inputs_->reportNaString());
        }
	CONSOLE("OK");
    }

    QuantLib::ext::shared_ptr<SimmConfiguration> simmConfiguration = inputs_->getSimmConfiguration();
    LOG("SIMM configuration created");
    MEM_LOG;

    LOG("Create CrifMarket");
    QuantLib::ext::shared_ptr<CrifMarket> crifMarket = QuantLib::ext::make_shared<CrifMarket>(
        inputs_->asof(), sensiAnalysis->simMarket(), analytic()->configurations().sensiScenarioData,
        analytic()->configurations().curveConfig);
    LOG("CrifMarket created");

    LOG("Generate CRIF report");
    CONSOLEW("CRIF: Generate Report");
    string baseCcy = crifAnalytic->baseCurrency();
    Real usdSpot = baseCcy == "USD" ? 1.0 : analytic()->market()->fxRate(baseCcy + "USD")->value();
    QuantLib::ext::shared_ptr<InMemoryReport> crifReport = QuantLib::ext::make_shared<InMemoryReport>();
    auto crif = crifAnalytic->computeCrif(portfolioSimmExemptions, ss, inputs_, crifMarket, usdSpot);
    crifAnalytic->crif() = crif;
    ReportWriter(inputs_->reportNaString()).writeCrifReport(crifReport, crifAnalytic->crif());
    analytic()->addReport(LABEL, "crif", crifReport);
    CONSOLE("OK");
    LOG("CRIF report generated successfully");
}

CrifAnalytic::CrifAnalytic(const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs,
                           const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager,
                           const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                           const std::string& baseCurrency)
    : Analytic(std::make_unique<CrifAnalyticImpl>(inputs), {"CRIF"}, inputs, analyticsManager) {
    portfolio_ = portfolio;
    baseCurrency_ = baseCurrency.empty() ? inputs->baseCurrency() : baseCurrency;
}

QuantLib::ext::shared_ptr<ore::analytics::Crif>
CrifAnalytic::computeCrif(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                          const QuantLib::ext::shared_ptr<SensitivityStream>& sensiStream,
                          const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                          const QuantLib::ext::shared_ptr<CrifMarket>& crifMarket, double usdSpot) {
    startTimer("computeCrif()");
    if (portfolio != nullptr && portfolio->size() > 0) {
        SimmTradeData tradeData(portfolio, market(), inputs->refDataManager(), inputs->simmBucketMapper());
        CrifGenerator crifGenerator(inputs->getSimmConfiguration(), inputs->simmNameMapper(), tradeData, crifMarket,
                                    inputs->xbsParConversion(), baseCurrency(), usdSpot, nullptr,
                                    inputs->refDataManager(), inputs->curveConfigs().get());
        stopTimer("computeCrif()");
        try {
            return crifGenerator.generateCrif(sensiStream);
        } catch (std::exception& e) {
            StructuredAnalyticsErrorMessage("CRIF Generation", "Failed to generate CRIF", e.what(), {}).log();
        }
    }
    stopTimer("computeCrif()");
    return QuantLib::ext::make_shared<ore::analytics::Crif>();
}
} // namespace analytics
}

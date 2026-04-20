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
#include <orea/app/analytics/pricinganalytic.hpp>
#include <orea/app/inputparameters.hpp>
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
#include <ored/report/inmemoryreport.hpp>
#include <ored/utilities/parsers.hpp>

using namespace ore::data;
using namespace std::filesystem;

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
            inputs->sensiRecalibrateModels(), inputs->sensiLaxFxConversion(), analytic->configurations().curveConfig,
            analytic->configurations().todaysMarketParams, false, inputs->refDataManager(),
            inputs->iborFallbackConfig(), true, inputs->dryRun(), inputs->useAtParCouponsTrades(), 
            inputs->computeTheta(), inputs->thetaPeriod());
    } else {
        sensiAnalysis = QuantLib::ext::make_shared<SensitivityAnalysis>(
            inputs->nThreads(), inputs->asof(), analytic->loader(), portfolio, Market::defaultConfiguration,
            inputs->pricingEngine(), analytic->configurations().simMarketParams,
            analytic->configurations().sensiScenarioData, inputs->sensiRecalibrateModels(),
            inputs->sensiLaxFxConversion(), analytic->configurations().curveConfig,
            analytic->configurations().todaysMarketParams, false, inputs->refDataManager(),
            inputs->iborFallbackConfig(), true, inputs->dryRun(), "analytic/" + analytic->label(),
            inputs->useAtParCouponsCurves(), inputs->useAtParCouponsTrades(),
            inputs->computeTheta(), inputs->thetaPeriod());
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
        sensiReports["crif_scenario"] = simmScenarioReport;
    } else {
        LOG("Skipping SIMM scenario report, this is an optional report and writeOptionalReports is set to false");
    }
    MEM_LOG;

    QuantLib::ext::shared_ptr<SensitivityStream> ss = QuantLib::ext::make_shared<SensitivityCubeStream>(
        sensiAnalysis->sensiCubes(), analytic->configurations().simMarketParams->baseCcy(), portfolio);
    if (writeReports) {
        auto simmSensitivityReport = QuantLib::ext::make_shared<InMemoryReport>();
        reportWriter.writeSensitivityReport(*simmSensitivityReport, ss, inputs->sensiThreshold(), analytic->market(),
                                            Market::defaultConfiguration, inputs->sensiOutputPrecision());
        sensiReports["crif_sensitivity"] = simmSensitivityReport;
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
        sensiReports["crif_sensitivity_config"] = simmSensitivityConfigReport;
    } else {
        LOG("Skipping SIMM sensitivity config report, this is an optional report and writeOptionalReports is set "
            "to false");
    }
    MEM_LOG;

    parAnalysis->computeParInstrumentSensitivities(sensiAnalysis->simMarket());
    if (writeReports) {
        QuantLib::ext::shared_ptr<InMemoryReport> parScenarioRatesReport =
            QuantLib::ext::make_shared<InMemoryReport>(inputs->reportBufferSize());
        parAnalysis->writeParRatesReport(*parScenarioRatesReport);
        sensiReports["crif_scenario_par_rates"] = parScenarioRatesReport;
    }
    QuantLib::ext::shared_ptr<ParSensitivityConverter> parConverter =
        QuantLib::ext::make_shared<ParSensitivityConverter>(parAnalysis->parSensitivities(), parAnalysis->shiftSizes());
    auto parCube = QuantLib::ext::make_shared<ZeroToParCube>(sensiAnalysis->sensiCubes(), parConverter, typesDisabled, true);
    ss = QuantLib::ext::make_shared<ParSensitivityCubeStream>(
        parCube, analytic->configurations().simMarketParams->baseCcy(), portfolio);
    // The stream will be reused for the crif generation, so we wrap it into a buffered stream to gain some
    // performance. The cost for this is the memory footpring of the buffer.
    ss = QuantLib::ext::make_shared<ore::analytics::BufferedSensitivityStream>(ss);
    if (writeReports) {
        auto simmParSensitivityReport = QuantLib::ext::make_shared<InMemoryReport>();
        reportWriter.writeSensitivityReport(*simmParSensitivityReport, ss, inputs->sensiThreshold(), analytic->market(),
                                            Market::defaultConfiguration, inputs->sensiOutputPrecision());
        sensiReports["crif_par_sensitivity"] = simmParSensitivityReport;
    }
    MEM_LOG;

    if (writeReports && inputs->outputJacobi()) {
        auto jacobiReport = QuantLib::ext::make_shared<InMemoryReport>();
        writeParConversionMatrix(parAnalysis->parSensitivities(), *jacobiReport);
        sensiReports["crif_par_conversion_matrix"] = jacobiReport;

        auto jacobiInverseReport = QuantLib::ext::make_shared<InMemoryReport>();
        parConverter->writeConversionMatrix(*jacobiInverseReport);
        sensiReports["crif_par_conversion_matrix_inverse"] = jacobiInverseReport;
    }

    analytic->stopTimer("computeSensitivities()");

    return std::make_pair(ss, sensiReports);
};
  
void CrifAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->sensiSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->sensiScenarioData();

    QL_REQUIRE(analytic()->configurations().simMarketParams, "CrifAnalytic: simMarketParams not set");
    QL_REQUIRE(analytic()->configurations().sensiScenarioData, "CrifAnalytic: sensiScenarioData not set");
    QL_REQUIRE(analytic()->configurations().todaysMarketParams, "CrifAnalytic: todaysMarketParams not set");

    inputs_->loadParameter<bool>(applySimmExemptions_, "crif", "applySimmExemptions", false,
                                 std::function<bool(const string&)>(parseBool));

    setGenerateAdditionalResults(true);
}

void CrifAnalyticImpl::buildDependencies() { 
    auto sensiAnalytic =
                AnalyticFactory::instance().build("SENSITIVITY", inputs_, analytic()->analyticsManager(), false);
    addDependentAnalytic("SENSITIVITY", sensiAnalytic.second);
}

void CrifAnalyticImpl::handlePreSimmExemptionsReports(
    CrifAnalyticBase& crifAnalytic, const QuantLib::ext::shared_ptr<InputParameters>& inputs,
    const std::string& marketConfig, const QuantLib::ext::shared_ptr<InMemoryReport>& npvWithoutReport) {}

bool CrifAnalyticImpl::continueWithEmptyPortfolio(const CrifAnalyticBase& crifAnalytic) const {
    return false;
}

void CrifAnalyticImpl::handlePostSimmExemptionsReports(
    CrifAnalyticBase& crifAnalytic, const QuantLib::ext::shared_ptr<InputParameters>& inputs,
    const std::string& marketConfig, const QuantLib::ext::shared_ptr<InMemoryReport>& npvWithReport,
    const QuantLib::ext::shared_ptr<InMemoryReport>& cfWithReport) {
    LOG("Write portfolio, with SIMM exemptions, to XML");
    path portfolioXmlPath = inputs->resultsPath() / "portfolio_with_simm_exemptions.xml";
    crifAnalytic.portfolioSimmExemptions()->toFile(portfolioXmlPath.string());
}

QuantLib::ext::shared_ptr<Portfolio> CrifAnalyticImpl::buildSimmExemptionOverridePortfolio(
    CrifAnalyticBase& crifAnalytic, const QuantLib::ext::shared_ptr<InputParameters>& inputs,
    std::set<std::string>& removedTrades, std::set<std::string>& modifiedTrades) {
    return nullptr;
}

QuantLib::ext::shared_ptr<SensitivityStream> CrifAnalyticImpl::extractParSensitivityStream(
    const QuantLib::ext::shared_ptr<Analytic>& sensiAnalytic,
    const QuantLib::ext::shared_ptr<Portfolio>& portfolio) const {
    auto pricingImpl = static_cast<PricingAnalyticImpl*>(sensiAnalytic->impl().get());
    auto sa = pricingImpl->sensiAnalysis();
    auto pa = pricingImpl->parAnalysis();
    auto baseCurrency = sa->simMarketData()->baseCcy();
    const auto& typesDisabled = analytic()->configurations().sensiScenarioData->parConversionExcludes();
    auto parConverter = QuantLib::ext::make_shared<ParSensitivityConverter>(pa->parSensitivities(), pa->shiftSizes());
    auto parCube = QuantLib::ext::make_shared<ZeroToParCube>(sa->sensiCubes(), parConverter, typesDisabled, true);
    auto stream = QuantLib::ext::make_shared<ParSensitivityCubeStream>(parCube, baseCurrency, portfolio);
    return QuantLib::ext::make_shared<ore::analytics::BufferedSensitivityStream>(stream);
}

QuantLib::ext::shared_ptr<SensitivityStream> CrifAnalyticImpl::computeExtraSensitivityStream(
    CrifAnalyticBase& crifAnalytic, const QuantLib::ext::shared_ptr<InputParameters>& inputs,
    const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
    const QuantLib::ext::shared_ptr<Analytic>& sensiAnalytic,
    const QuantLib::ext::shared_ptr<Portfolio>& simmOverridesPortfolio) {
    return nullptr;
}

void CrifAnalyticImpl::handleMainSensitivityReports(CrifAnalyticBase& crifAnalytic,
                                                    const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                                    const QuantLib::ext::shared_ptr<Analytic>& sensiAnalytic) {
    static const std::vector<std::pair<std::string, std::string>> reportMapping = {
        {"sensitivity_scenario", "crif_scenario"},
        {"sensitivity", "crif_sensitivity"},
        {"sensitivity_config", "crif_sensitivity_config"},
        {"par_sensitivity", "crif_par_sensitivity"},
        {"jacobi", "crif_par_conversion_matrix"},
        {"jacobi_inverse", "crif_par_conversion_matrix_inverse"},
        {"scenario_par_rates", "crif_scenario_par_rates"}};
    auto sensiReports = sensiAnalytic->reports();
    if (sensiReports.find("SENSITIVITY") != sensiReports.end()) {
        for (const auto& [srcName, dstName] : reportMapping) {
            auto it = sensiReports.at("SENSITIVITY").find(srcName);
            if (it != sensiReports.at("SENSITIVITY").end())
                analytic()->addReport(LABEL, dstName, it->second);
        }
    }
}

QuantLib::ext::shared_ptr<PortfolioFieldGetter> CrifAnalyticImpl::buildPortfolioFieldGetter(
    CrifAnalyticBase& crifAnalytic, const QuantLib::ext::shared_ptr<InputParameters>& inputs,
    const QuantLib::ext::shared_ptr<Portfolio>& simmOverridesPortfolio) {
    return nullptr;
}

void CrifAnalyticImpl::extendCrif(CrifAnalyticBase& crifAnalytic,
                                  const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                  const QuantLib::ext::shared_ptr<Crif>& crif,
                                  const QuantLib::ext::shared_ptr<Portfolio>& simmOverridesPortfolio,
                                  const QuantLib::ext::shared_ptr<SensitivityStream>& ssSimmOverrides,
                                  const std::set<std::string>& removedTrades,
                                  const std::set<std::string>& modifiedTrades,
                                  const QuantLib::ext::shared_ptr<CrifMarket>& crifMarket,
                                  const QuantLib::ext::shared_ptr<PortfolioFieldGetter>& fieldGetter,
                                  double usdSpot) {}

void CrifAnalyticImpl::writeCrifReport(CrifAnalyticBase& crifAnalytic,
                                       const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                       const QuantLib::ext::shared_ptr<InMemoryReport>& crifReport,
                                       const QuantLib::ext::shared_ptr<Crif>& crif,
                                       const QuantLib::ext::shared_ptr<PortfolioFieldGetter>& fieldGetter) {
    ReportWriter(inputs->reportNaString()).writeCrifReport(crifReport, crif);
}

void CrifAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                   const std::set<std::string>& runTypes) {
    QL_REQUIRE(analytic()->portfolio(), "CrifAnalytic::run: No portfolio loaded.");

    CONSOLEW("CRIF: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    CONSOLEW("CRIF: Build Portfolio");
    analytic()->buildPortfolio();
    CONSOLE("OK");

    ObservationMode::instance().setMode(ObservationMode::Mode::None);

    auto crifAnalytic = dynamic_cast<CrifAnalyticBase*>(analytic());
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
    handlePreSimmExemptionsReports(*crifAnalytic, inputs_, marketConfig, npvWithoutReport);

    std::set<std::string> removedTrades, modifiedTrades;
    if (applySimmExemptions_) {
        analytic()->startTimer("applySimmExemptions()");
        try {
            std::tie(removedTrades, modifiedTrades) =
                applySimmExemptions(*analytic()->portfolio(), engineFactory(), crifAnalytic->simmExemptionOverrides(),
                                    inputs_->useAtParCouponsTrades());
        } catch (std::exception& e) {
            QL_FAIL(e.what());
        }
        analytic()->stopTimer("applySimmExemptions()");
    } else {
        WLOG("Skipping application of SIMM exemptions as applySimmExemptions is set to false");
    }

    // If we have an empty portfolio, then quit the CRIF analytic
    if (analytic()->portfolio()->size() == 0 && !continueWithEmptyPortfolio(*crifAnalytic)) {
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
    handlePostSimmExemptionsReports(*crifAnalytic, inputs_, marketConfig, npvWithReport, cfWithReport);

    auto simmOverridesPortfolio =
        buildSimmExemptionOverridePortfolio(*crifAnalytic, inputs_, removedTrades, modifiedTrades);
    if (analytic()->portfolio()->size() == 0 &&
        (!simmOverridesPortfolio || simmOverridesPortfolio->size() == 0)) {
        ALOG("portfolio is empty once SIMM exemptions applied");
        analytic()->addReport(LABEL, "crif", QuantLib::ext::make_shared<InMemoryReport>());
        return;
    }

    // Run the dependent SENSITIVITY analytic
    auto sensiAnalytic = dependentAnalytic(sensitivityLookUpKey);

    // Override the dependent analytic's configurations with the CRIF-specific ones
    sensiAnalytic->configurations().todaysMarketParams = analytic()->configurations().todaysMarketParams;
    sensiAnalytic->configurations().simMarketParams = analytic()->configurations().simMarketParams;
    sensiAnalytic->configurations().sensiScenarioData = analytic()->configurations().sensiScenarioData;

    QuantLib::ext::shared_ptr<SensitivityStream> ssSimmOverrides =
        computeExtraSensitivityStream(*crifAnalytic, inputs_, loader, sensiAnalytic, simmOverridesPortfolio);
    QuantLib::ext::shared_ptr<SensitivityStream> ss;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityAnalysis> sensiAnalysis;
    if (portfolioSimmExemptions->size() > 0) {
        LOG("Begin sensitivity and par sensitivity analysis");
        CONSOLEW("CRIF: Run Sensitivity");

        // Get the dependent SENSITIVITY analytic and set the SIMM-exempted portfolio
        sensiAnalytic->setPortfolio(portfolioSimmExemptions);

        // Ensure par sensitivity conversion and pillar alignment are enabled (required for CRIF)
        bool savedParSensi = inputs_->parSensi();
        bool savedAlignPillars = inputs_->alignPillars();
        inputs_->setParSensi(true);
        inputs_->setAlignPillars(true);

        sensiAnalytic->runAnalytic(loader, {"SENSITIVITY"});

        inputs_->setParSensi(savedParSensi);
        inputs_->setAlignPillars(savedAlignPillars);

        // Extract sensi analysis and par analysis from the PricingAnalyticImpl
        auto pricingImpl = static_cast<PricingAnalyticImpl*>(sensiAnalytic->impl().get());
        sensiAnalysis = pricingImpl->sensiAnalysis();
        ss = extractParSensitivityStream(sensiAnalytic, portfolioSimmExemptions);

        LOG("Finished sensitivity and par sensitivity analysis");
        handleMainSensitivityReports(*crifAnalytic, inputs_, sensiAnalytic);

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
    auto fieldGetter = buildPortfolioFieldGetter(*crifAnalytic, inputs_, simmOverridesPortfolio);
    auto crif = crifAnalytic->computeCrif(portfolioSimmExemptions, ss, inputs_, false, removedTrades, modifiedTrades,
                                          crifMarket, fieldGetter, usdSpot);
    extendCrif(*crifAnalytic, inputs_, crif, simmOverridesPortfolio, ssSimmOverrides, removedTrades, modifiedTrades,
               crifMarket, fieldGetter, usdSpot);
    crifAnalytic->crif() = crif;
    writeCrifReport(*crifAnalytic, inputs_, crifReport, crifAnalytic->crif(), fieldGetter);
    analytic()->addReport(LABEL, "crif", crifReport);
    CONSOLE("OK");
    LOG("CRIF report generated successfully");
}

CrifAnalytic::CrifAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                           const QuantLib::ext::weak_ptr<AnalyticsManager>& analyticsManager,
                           const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                           const std::string& baseCurrency)
    : Analytic(std::make_unique<CrifAnalyticImpl>(inputs), {"CRIF"}, inputs, analyticsManager) {
    portfolio_ = portfolio;
    baseCurrency_ = baseCurrency.empty() ? inputs->baseCurrency() : baseCurrency;
}

QuantLib::ext::shared_ptr<Crif>
CrifAnalytic::computeCrif(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                          const QuantLib::ext::shared_ptr<SensitivityStream>& sensiStream,
                          const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                          bool isSimmOverrideExceptionPortfolio,
                          const std::set<std::string>& removedTrades,
                          const std::set<std::string>& modifiedTrades,
                          const QuantLib::ext::shared_ptr<CrifMarket>& crifMarket,
                          const QuantLib::ext::shared_ptr<PortfolioFieldGetter>& fieldGetter, double usdSpot) {
    startTimer("computeCrif()");
    if (portfolio != nullptr && portfolio->size() > 0) {
        auto tradeData = QuantLib::ext::make_shared<SimmTradeData>(portfolio, market(), inputs->refDataManager(),
								   inputs->simmBucketMapper());
        tradeData->init();
        CrifGenerator crifGenerator(inputs->getSimmConfiguration(), inputs->simmNameMapper(), tradeData, crifMarket,
                                    inputs->xbsParConversion(), baseCurrency(), usdSpot, fieldGetter,
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

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

#include <orea/app/analytics/pnlanalytic.hpp>
#include <orea/app/analytics/scenarioanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/scenario/simplescenario.hpp>
#include <orea/scenario/scenariowriter.hpp>
#include <orea/scenario/scenarioutilities.hpp>

#include <ored/marketdata/structuredcurveerror.hpp>

using RFType = ore::analytics::RiskFactorKey::KeyType;

namespace ore {
namespace analytics {

void PnlAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().simulationConfigRequired = true;

    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->scenarioSimMarketParams();

    setGenerateAdditionalResults(true);
}

void PnlAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
    const std::set<std::string>& runTypes) {
    
    if (!analytic()->match(runTypes))
        return;

    Settings::instance().evaluationDate() = inputs_->asof();
    analytic()->configurations().asofDate = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());

    QL_REQUIRE(inputs_->portfolio(), "PnlAnalytic::run: No portfolio loaded.");
    QL_REQUIRE(inputs_->portfolio()->size() > 0, "PnlAnalytic::run: Portfolio is empty.");

    std::string effectiveResultCurrency =
        inputs_->resultCurrency().empty() ? inputs_->baseCurrency() : inputs_->resultCurrency();

    /*******************************
     *
     * 0. Build market and portfolio
     *
     *******************************/

    analytic()->buildMarket(loader);

    /****************************************************
     *
     * 1. Write the t0 NPV and Additional Results reports
     *
     ****************************************************/

    // Build a simMarket on the asof date
    QL_REQUIRE(analytic()->configurations().simMarketParams, "scenario sim market parameters not set");
    QL_REQUIRE(analytic()->configurations().todaysMarketParams, "today's market parameters not set");
    
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        analytic()->market(), analytic()->configurations().simMarketParams, inputs_->marketConfig("pricing"),
        *analytic()->configurations().curveConfig, *analytic()->configurations().todaysMarketParams,
        inputs_->continueOnError(), useSpreadedTermStructures(), false, false, *inputs_->iborFallbackConfig());
    auto sgen = QuantLib::ext::make_shared<StaticScenarioGenerator>();
    simMarket->scenarioGenerator() = sgen;

    analytic()->setMarket(simMarket);
    analytic()->buildPortfolio();

    boost::shared_ptr<InMemoryReport> t0NpvReport = boost::make_shared<InMemoryReport>();
    ReportWriter(inputs_->reportNaString())
        .writeNpv(*t0NpvReport, effectiveResultCurrency, analytic()->market(), inputs_->marketConfig("pricing"),
                  analytic()->portfolio());
    analytic()->reports()[LABEL]["pnl_npv_t0"] = t0NpvReport;

    if (inputs_->outputAdditionalResults()) {
        CONSOLEW("Pricing: Additional t0 Results");
	boost::shared_ptr<InMemoryReport> t0AddReport = boost::make_shared<InMemoryReport>();
        ReportWriter(inputs_->reportNaString())
            .writeAdditionalResultsReport(*t0AddReport, analytic()->portfolio(), analytic()->market(),
                                          effectiveResultCurrency);
        analytic()->reports()[LABEL]["pnl_additional_results_t0"] = t0AddReport;
        CONSOLE("OK");
    }

    /****************************************************
     *
     * 2. Write cash flow report for the clean actual P&L 
     *
     ****************************************************/

    boost::shared_ptr<InMemoryReport> t0CashFlowReport = boost::make_shared<InMemoryReport>();
    string marketConfig = inputs_->marketConfig("pricing");
    ReportWriter(inputs_->reportNaString())
      .writeCashflow(*t0CashFlowReport, effectiveResultCurrency, analytic()->portfolio(),
		     analytic()->market(),
		     marketConfig, inputs_->includePastCashflows());
    analytic()->reports()[LABEL]["pnl_cashflow"] = t0CashFlowReport;
    
    /*******************************************************************************************
     *
     * 3. Prepare "NPV lagged" calculations by creating shift scenarios
     *    - to price the t0 portfolio as of t0 using the t1 market (risk hypothetical clean P&L)
     *    - to price the t0 portfolio as of t1 using the t0 market (theta, time decay)
     *
     *******************************************************************************************/

    // Set eveluationDate to t1 > t0
    Settings::instance().evaluationDate() = mporDate();

    // Set the configurations asof date for the mpor analytic to t1, too
    auto mporAnalytic = dependentAnalytic(mporLookupKey);
    mporAnalytic->configurations().asofDate = mporDate();
    mporAnalytic->configurations().todaysMarketParams = analytic()->configurations().todaysMarketParams;
    mporAnalytic->configurations().simMarketParams = analytic()->configurations().simMarketParams;

    // Run the mpor analytic to generate the market scenario as of t1
    mporAnalytic->runAnalytic(loader);

    // Set the evaluation date back to t0
    Settings::instance().evaluationDate() = inputs_->asof();
        
    QuantLib::ext::shared_ptr<Scenario> asofBaseScenario = simMarket->baseScenarioAbsolute();
    auto sai = static_cast<ScenarioAnalyticImpl*>(mporAnalytic->impl().get());
    QuantLib::ext::shared_ptr<Scenario> mporBaseScenario = sai->scenarioSimMarket()->baseScenarioAbsolute();

    QuantLib::ext::shared_ptr<ore::analytics::Scenario> t0Scenario =
        getDifferenceScenario(asofBaseScenario, mporBaseScenario, inputs_->asof(), 1.0); 
    setT0Scenario(asofBaseScenario);

    // Create the inverse shift scenario as spread between t0 market and t1 market, to be applied to t1

    QuantLib::ext::shared_ptr<ore::analytics::Scenario> t1Scenario =
        getDifferenceScenario(mporBaseScenario, asofBaseScenario, mporDate(), 1.0); 
    setT1Scenario(mporBaseScenario);

    /********************************************************************************************
     *
     * 4. Price the t0 portfolio as of t0 using the t1 market for the risk-hypothetical clean P&L
     *
     ********************************************************************************************/

    // Now update simMarket on asof date t0, with the t0 shift scenario
    sgen->setScenario(t0Scenario);
    simMarket->update(simMarket->asofDate());
    analytic()->setMarket(simMarket);

    // Build the portfolio, linked to the shifted market
    analytic()->buildPortfolio();

    // This hook allows modifying the portfolio in derived classes before running the analytics below
    analytic()->modifyPortfolio();
    
    QuantLib::ext::shared_ptr<InMemoryReport> t0NpvLaggedReport = QuantLib::ext::make_shared<InMemoryReport>();
    ReportWriter(inputs_->reportNaString())
        .writeNpv(*t0NpvLaggedReport, effectiveResultCurrency, analytic()->market(), inputs_->marketConfig("pricing"),
                  analytic()->portfolio());

    if (inputs_->outputAdditionalResults()) {
        CONSOLEW("Pricing: Additional Results, t0 lagged");
        boost::shared_ptr<InMemoryReport> t0LaggedAddReport = boost::make_shared<InMemoryReport>();
        ReportWriter(inputs_->reportNaString())
            .writeAdditionalResultsReport(*t0LaggedAddReport, analytic()->portfolio(), analytic()->market(),
                                          effectiveResultCurrency);
        analytic()->reports()[LABEL]["pnl_additional_results_lagged_t0"] = t0LaggedAddReport;
        CONSOLE("OK");
    }
    analytic()->reports()[LABEL]["pnl_npv_lagged_t0"] = t0NpvLaggedReport;

    /***********************************************************************************************
     *
     * 5. Price the t0 portfolio as of t1 using the t0 market for the theta / time decay calculation
     *    Reusing the mpor analytic setup here which is as of t1 already.
     *
     ***********************************************************************************************/

    Date d1 = mporDate();
    Settings::instance().evaluationDate() = d1;
    analytic()->configurations().asofDate = d1;
    auto simMarket1 = sai->scenarioSimMarket();
    auto sgen1 = QuantLib::ext::make_shared<StaticScenarioGenerator>();
    analytic()->setMarket(simMarket1);
    sgen1->setScenario(t1Scenario);
    simMarket1->scenarioGenerator() = sgen1;
    simMarket1->update(d1);
    analytic()->buildPortfolio();

    QuantLib::ext::shared_ptr<InMemoryReport> t1NpvLaggedReport = QuantLib::ext::make_shared<InMemoryReport>();
    ReportWriter(inputs_->reportNaString())
        .writeNpv(*t1NpvLaggedReport, effectiveResultCurrency, analytic()->market(), inputs_->marketConfig("pricing"),
                  analytic()->portfolio());

    analytic()->reports()[LABEL]["pnl_npv_lagged_t1"] = t1NpvLaggedReport;

    if (inputs_->outputAdditionalResults()) {
        CONSOLEW("Pricing: Additional Results t1");
        boost::shared_ptr<InMemoryReport> t1AddReport = boost::make_shared<InMemoryReport>();
        ReportWriter(inputs_->reportNaString())
            .writeAdditionalResultsReport(*t1AddReport, analytic()->portfolio(), analytic()->market(),
                                          effectiveResultCurrency);
        analytic()->reports()[LABEL]["pnl_additional_results_lagged_t1"] = t1AddReport;
        CONSOLE("OK");
    }

    /***************************************************************************************
     *
     * 6. Price the t0 portfolio as of t1 using the t1 market for the actual P&L calculation
     *    TODO: This should use the t1 portfolio instead.
     *
     ***************************************************************************************/
        
    sgen1->setScenario(sai->scenarioSimMarket()->baseScenario());
    simMarket1->scenarioGenerator() = sgen1;
    simMarket1->update(d1);
    analytic()->buildPortfolio();

    QuantLib::ext::shared_ptr<InMemoryReport> t1NpvReport = QuantLib::ext::make_shared<InMemoryReport>();
    ReportWriter(inputs_->reportNaString())
        .writeNpv(*t1NpvReport, effectiveResultCurrency, analytic()->market(), inputs_->marketConfig("pricing"),
                  analytic()->portfolio());

    analytic()->reports()[LABEL]["pnl_npv_t1"] = t1NpvReport;

    if (inputs_->outputAdditionalResults()) {
        CONSOLEW("Pricing: Additional t1 Results");
	boost::shared_ptr<InMemoryReport> t1AddReport = boost::make_shared<InMemoryReport>();
        ReportWriter(inputs_->reportNaString())
            .writeAdditionalResultsReport(*t1AddReport, analytic()->portfolio(), analytic()->market(),
                                          effectiveResultCurrency);
        analytic()->reports()[LABEL]["pnl_additional_results_t1"] = t1AddReport;
        CONSOLE("OK");
    }

    /****************************
     *
     * 7. Generate the P&L report
     *
     ****************************/

    // FIXME: check which market and which portfolio to pass to the report writer
    QuantLib::ext::shared_ptr<InMemoryReport> pnlReport = QuantLib::ext::make_shared<InMemoryReport>();
    ReportWriter(inputs_->reportNaString())
        .writePnlReport(*pnlReport, t0NpvReport, t0NpvLaggedReport, t1NpvLaggedReport, t1NpvReport,
			t0CashFlowReport, inputs_->asof(), mporDate(),
			effectiveResultCurrency, analytic()->market(), inputs_->marketConfig("pricing"), analytic()->portfolio());
    analytic()->reports()[LABEL]["pnl"] = pnlReport;

    /***************************
     *
     * 8. Write Scenario Reports
     *
     ***************************/

    boost::shared_ptr<InMemoryReport> t0ScenarioReport = boost::make_shared<InMemoryReport>();
    auto t0sw = ScenarioWriter(nullptr, t0ScenarioReport);
    t0sw.writeScenario(asofBaseScenario, true);
    analytic()->reports()[label()]["pnl_scenario_t0"] = t0ScenarioReport;

    boost::shared_ptr<InMemoryReport> t1ScenarioReport = boost::make_shared<InMemoryReport>();
    auto t1sw = ScenarioWriter(nullptr, t1ScenarioReport);
    t1sw.writeScenario(mporBaseScenario, true);
    analytic()->reports()[label()]["pnl_scenario_t1"] = t1ScenarioReport;    
}

} // namespace analytics
} // namespace ore

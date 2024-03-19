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
#include <ored/marketdata/todaysmarket.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

/*******************************************************************
 * PRICING Analytic: NPV, CASHFLOW, CASHFLOWNPV, SENSITIVITY, STRESS
 *******************************************************************/

void PricingAnalyticImpl::setUpConfigurations() {    
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
                .writeNpv(*report, effectiveResultCurrency, analytic()->market(), inputs_->marketConfig("pricing"),
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
        else {
            QL_FAIL("PricingAnalytic type " << type << " invalid");
        }
    }
}

} // namespace analytics
} // namespace ore

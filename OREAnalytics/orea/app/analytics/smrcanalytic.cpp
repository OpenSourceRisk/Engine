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

#include <orea/app/analytics/smrcanalytic.hpp>
#include <orea/engine/smrc.hpp>
#include <orea/app/reportwriter.hpp>

namespace ore {
namespace analytics {

void SmrcAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
}

// SmrcAnalytic
SmrcAnalytic::SmrcAnalytic(const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs)
    : Analytic(std::make_unique<SmrcAnalyticImpl>(inputs), {"SMRC"}, inputs) {}

void SmrcAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                   const std::set<std::string>& runTypes) {

    if (!analytic()->match(runTypes))
        return;

    LOG("SmrcAnalytic::runAnalytic called");

    analytic()->buildMarket(loader);
    analytic()->buildPortfolio();

    analytic()->enrichIndexFixings(analytic()->portfolio());

    auto detailReport = QuantLib::ext::make_shared<InMemoryReport>();
    auto summaryReport = QuantLib::ext::make_shared<InMemoryReport>();
    auto smrc = QuantLib::ext::make_shared<SMRC>(analytic()->portfolio(), analytic()->market(), inputs_->baseCurrency(),
                                                 detailReport, summaryReport);

    auto marketConfig = inputs_->marketConfig("pricing");
    if (analytic()->getWriteIntermediateReports()) {
        if (inputs_->outputAdditionalResults()) {
            LOG("Write additional results for SMRC");
            boost::filesystem::path addResultsReportPath = inputs_->resultsPath() / "additional_results.csv";
            CSVFileReport addResultsReport(addResultsReportPath.string(), ',', false, inputs_->csvQuoteChar(),
                                           inputs_->reportNaString(), true);
            ReportWriter(inputs_->reportNaString())
                .writeAdditionalResultsReport(addResultsReport, analytic()->portfolio(), analytic()->market(),
                                              marketConfig, inputs_->baseCurrency());
        }

        LOG("Write cashflow report for SMRC");
        boost::filesystem::path cfReportPath = inputs_->resultsPath() / "cashflow.csv";
        CSVFileReport cfReport(cfReportPath.string(), ',', false, inputs_->csvQuoteChar(), inputs_->reportNaString(),
                               false);
        ReportWriter(inputs_->reportNaString())
            .writeCashflow(cfReport, inputs_->baseCurrency(), analytic()->portfolio(), analytic()->market(),
                           marketConfig);
    }

    analytic()->reports()[label()]["smrc"] = summaryReport;
    analytic()->reports()[label()]["smrcdetail"] = detailReport;
}

} // namespace analytics
} // namespace ore

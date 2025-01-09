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

#include <ored/portfolio/structuredconfigurationwarning.hpp>
#include <orea/app/analytics/saccranalytic.hpp>
#include <orea/app/reportwriter.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

SaCcrAnalyticImpl::SaCcrAnalyticImpl(const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs)
    : Analytic::Impl(inputs) {
    setLabel(LABEL);
    if (inputs->nettingSetManager())
        inputs->nettingSetManager()->loadAll();
}

void SaCcrAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().simulationConfigRequired = false;
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
}

void SaCcrAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<InMemoryLoader>& loader,
                                const std::set<std::string>& runTypes) {
    if (!analytic()->match(runTypes))
        return;

    LOG("SaCcrAnalytic::runAnalytic called");

    auto saccrAnalytic = static_cast<SaCcrAnalytic*>(analytic());
    QL_REQUIRE(saccrAnalytic, "Analytic must be of type SaCcrAnalytic");

    std::map<SACCR::ReportType, QuantLib::ext::shared_ptr<Report>> saccrReports;

    auto calculatedCollateralBalances = QuantLib::ext::make_shared<CollateralBalances>();

    analytic()->buildMarket(loader);
    analytic()->buildPortfolio();

    analytic()->enrichIndexFixings(analytic()->portfolio());

    auto marketConfig = inputs_->marketConfig("pricing");
    // For the additional results and cashflows, we do not take them from the CRIF analytic/s since we need these
    // for validation for all trades regardless of the CalculateIMAmount flag of each netting set
    if (analytic()->getWriteIntermediateReports()) {
        if (inputs_->outputAdditionalResults()) {
            LOG("Write additional results for SA-CCR");
            path addResultsReportPath = inputs_->resultsPath() / "additional_results.csv";
            CSVFileReport addResultsWithoutReport(addResultsReportPath.string(), ',', false, inputs_->csvQuoteChar(),
                                                  inputs_->reportNaString(), true);
            ReportWriter(inputs_->reportNaString())
                .writeAdditionalResultsReport(addResultsWithoutReport, analytic()->portfolio(), analytic()->market(),
                                              marketConfig, inputs_->baseCurrency());
        }

        LOG("Write cashflow report for SA-CCR");
        boost::filesystem::path cfReportPath = inputs_->resultsPath() / "cashflow.csv";
        CSVFileReport cfReport(cfReportPath.string(), ',', false, inputs_->csvQuoteChar(), inputs_->reportNaString(),
                               false);
        ReportWriter(inputs_->reportNaString())
            .writeCashflow(cfReport, inputs_->baseCurrency(), analytic()->portfolio(), analytic()->market(),
                           marketConfig);
    }
    
    // Load collateral balances if provided
    auto& collateralBalances =
        inputs_->collateralBalances() ? inputs_->collateralBalances()
                                                                 : QuantLib::ext::make_shared<CollateralBalances>();

    // Load netting set definitions if provided
    auto& nettingSetManager =
        inputs_->nettingSetManager() ? inputs_->nettingSetManager() : QuantLib::ext::make_shared<NettingSetManager>();

    // Load counterparty information if provided
    auto& counterpartyManager = inputs_->counterpartyManager() ? inputs_->counterpartyManager()
                                                                   : QuantLib::ext::make_shared<CounterpartyManager>();

    QuantLib::ext::shared_ptr<InMemoryReport> saCcrReport = QuantLib::ext::make_shared<InMemoryReport>();
    QuantLib::ext::shared_ptr<InMemoryReport> saCcrDetailReport = QuantLib::ext::make_shared<InMemoryReport>();

    saccrReports[SACCR::ReportType::Summary] = saCcrReport;
    saccrReports[SACCR::ReportType::Detail] = saCcrDetailReport;

    // Main SA-CCR calculation
    auto saccr = QuantLib::ext::make_shared<SACCR>(
        analytic()->portfolio(), nettingSetManager, counterpartyManager, analytic()->market(),
        inputs_->baseCurrency(), collateralBalances, calculatedCollateralBalances, inputs_->simmNameMapper(),
        inputs_->simmBucketMapper(), inputs_->refDataManager(), saccrReports);
    saccrAnalytic->setSaccr(saccr);

    // Write out the collateral balances that were (ultimately) used
    path p = inputs_->resultsPath() / "collateralbalances.xml";
    LOG("Saving collateral balances to file: " << p.string());
    collateralBalances->toFile(p.string());

    analytic()->reports()[label()]["saccr"] = saCcrReport;
    analytic()->reports()[label()]["saccr_detail"] = saCcrDetailReport;    
}

}
}

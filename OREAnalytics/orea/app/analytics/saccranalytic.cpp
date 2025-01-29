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
#include <orea/engine/saccrtradedata.hpp>
#include <orea/engine/saccrcrifgenerator.hpp>
#include <orea/engine/saccrcalculator.hpp>

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

    std::map<SaccrCalculator::ReportType, QuantLib::ext::shared_ptr<Report>> saccrReports;

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

    saccrReports[SaccrCalculator::ReportType::Summary] = saCcrReport;

    analytic()->startTimer("Build SA-CCR trade data");
    auto saccrTradeData = QuantLib::ext::make_shared<SaccrTradeData>(
        analytic()->market(), inputs_->simmNameMapper(), inputs_->simmBucketMapper(),
        inputs_->refDataManager(), inputs_->baseCurrency(), inputs_->reportNaString(), nettingSetManager,
        counterpartyManager, collateralBalances, calculatedCollateralBalances);
    saccrTradeData->initialise(analytic()->portfolio());
    analytic()->stopTimer("Build SA-CCR trade data");
    saccrAnalytic->setSaccrTradeData(saccrTradeData);

    // Create capital CRIF
    analytic()->startTimer("Capital CRIF generation");
    auto saccrCrifGenerator = QuantLib::ext::make_shared<SaccrCrifGenerator>(saccrTradeData);
    auto saccrCrif = saccrCrifGenerator->generateCrif();

    auto crifReport = QuantLib::ext::make_shared<InMemoryReport>();
    path crifReportPath = inputs_->resultsPath() / "capital_crif.csv";
    ReportWriter(inputs_->reportNaString())
        .writeCapitalCrifReport(*crifReport, saccrCrif, inputs_->baseCurrency(), inputs_->csvQuoteChar());
    crifReport->toFile(crifReportPath.string(), ',', false, inputs_->csvQuoteChar(), inputs_->reportNaString());
    analytic()->stopTimer("Capital CRIF generation");

    QuantLib::ext::shared_ptr<InMemoryReport> saccrDetailReport = QuantLib::ext::make_shared<InMemoryReport>();
    ReportWriter(inputs_->reportNaString()).writeSaccrTradeDetailReport(*saccrDetailReport, saccrTradeData);
    if (saccrAnalytic->getWriteIntermediateReports()) {
        path saccrDetailPath = inputs_->resultsPath() / "saccrdetail.csv";
        saccrDetailReport->toFile(saccrDetailPath.string(), ',', false, inputs_->csvQuoteChar(),
                                  inputs_->reportNaString());
    }

    // Main SA-CCR calculation
    analytic()->startTimer("SA-CCR calculation");
    auto saccrCalculator = QuantLib::ext::make_shared<SaccrCalculator>(
        saccrCrif, saccrTradeData, inputs_->baseCurrency(), nettingSetManager, counterpartyManager,
        analytic()->market(), saccrReports);
    analytic()->stopTimer("SA-CCR calculation");
    saccrAnalytic->setSaccrCalculator(saccrCalculator);

    // Write out SA-CCR summary and detailed reports
    if (saccrAnalytic->getWriteIntermediateReports()) {
        path saCcrPath = inputs_->resultsPath() / "saccr.csv";
        saCcrReport->toFile(saCcrPath.string(), ',', false, inputs_->csvQuoteChar(), inputs_->reportNaString());
    }

    analytic()->reports()[label()]["saccr"] = saCcrReport;
    analytic()->reports()[label()]["saccr_detail"] = saccrDetailReport;
}

}
}

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

#include <orea/app/analytics/imscheduleanalytic.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

void IMScheduleAnalytic::loadCrifRecords(const boost::shared_ptr<ore::data::InMemoryLoader>& loader) {
    QL_REQUIRE(inputs_, "Inputs not set");
    QL_REQUIRE(!inputs_->crif().empty(), "CRIF loader does not contain any records");
        
    crif_ = inputs_->crif();
    crif_.fillAmountUsd(market());
    hasNettingSetDetails_ = crif_.hasNettingSetDetails();
}

void IMScheduleAnalyticImpl::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                         const std::set<std::string>& runTypes) {

    if (!analytic()->match(runTypes))
        return;

    LOG("IMScheduleAnalytic::runAnalytic called");

    analytic()->buildMarket(loader, false);

    auto imAnalytic = static_cast<IMScheduleAnalytic*>(analytic());
    QL_REQUIRE(imAnalytic, "Analytic must be of type IMScheduleAnalytic");

    /*
    imAnalytic->loadCrifRecords(loader);

    // Calculate IMSchedule
    LOG("Calculating Schedule IM")
    auto imSchedule = boost::make_shared<IMScheduleCalculator>(
        imAnalytic->crif().get_value_or(Crif()), inputs->simmResultCurrency(), analytic()->market(),
        imAnalytic->imScheduleOnly(), plusInputs->enforceIMRegulations(), false, imAnalytic->hasSEC(),
        imAnalytic->hasCFTC());
    imAnalytic->setImSchedule(imSchedule);

    if (imAnalytic->imScheduleOnly()) {
        Real fxSpotReport = 1.0;
        if (!plusInputs->simmReportingCurrency().empty()) {
            fxSpotReport = analytic()
                               ->market()
                               ->fxRate(plusInputs->simmResultCurrency() + plusInputs->simmReportingCurrency())
                               ->value();
            DLOG("SIMM reporting currency is " << plusInputs->simmReportingCurrency() << " with fxSpot "
                                               << fxSpotReport);
        }

        boost::shared_ptr<InMemoryReport> imScheduleSummaryReport = boost::make_shared<InMemoryReport>();
        boost::shared_ptr<InMemoryReport> imScheduleTradeReport = boost::make_shared<InMemoryReport>();

        // Populate the trade-level IM Schedule report
        LOG("Generating Schedule IM reports")
        oreplus::analytics::ReportWriter(inputs_->reportNaString())
            .writeIMScheduleTradeReport(imSchedule->imScheduleTradeResults(), imScheduleTradeReport,
                                        imAnalytic->hasNettingSetDetails());

        // Populate the netting set level IM Schedule report
        oreplus::analytics::ReportWriter(inputs_->reportNaString())
            .writeIMScheduleSummaryReport(imSchedule->finalImScheduleSummaryResults(), imScheduleSummaryReport,
                                          imAnalytic->hasNettingSetDetails(), plusInputs->simmResultCurrency(),
                                          plusInputs->simmReportingCurrency(), fxSpotReport);

        // Write out reports
        if (analytic()->getWriteIntermediateReports()) {
            path imScheduleTradePath = inputs_->resultsPath() / "imschedule_detail.csv";
            imScheduleTradeReport->toFile(imScheduleTradePath.string(), ',', false, inputs_->csvQuoteChar(),
                                          inputs_->reportNaString());
            LOG("Trade-level IM Schedule report generated.")

            path imScheduleSummaryPath = inputs_->resultsPath() / "imschedule.csv";
            imScheduleSummaryReport->toFile(imScheduleSummaryPath.string(), ',', false, inputs_->csvQuoteChar(),
                                            inputs_->reportNaString());
            LOG("Netting set level IM Schedule report generated.")
        }
        LOG("Schedule IM reports generated");
        MEM_LOG;

        analytic()->reports()["IM_SCHEDULE"]["im_schedule"] = imScheduleSummaryReport;
    */
}

} // namespace analytics
} // namespace ore

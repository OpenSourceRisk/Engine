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
#include <orea/simm/simmconfiguration.hpp>
#include <orea/app/reportwriter.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

void IMScheduleAnalytic::loadCrifRecords(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {
    QL_REQUIRE(inputs_, "Inputs not set");
    QL_REQUIRE(!inputs_->crif().empty(), "CRIF loader does not contain any records");
        
    crif_ = inputs_->crif();
    crif_.fillAmountUsd(market());
    hasNettingSetDetails_ = crif_.hasNettingSetDetails();

    // Keep record of which netting sets have SEC and CFTC
    map<string, bool> hasSECCache, hasCFTCCache;
    for (const CrifRecord& cr : crif_) {
        const NettingSetDetails& nsd = cr.nettingSetDetails;

        for (const SimmConfiguration::SimmSide& side : {SimmConfiguration::SimmSide::Call, SimmConfiguration::SimmSide::Post}) {
            const string& crifRegs = side == SimmConfiguration::SimmSide::Call ? cr.collectRegulations : cr.postRegulations;
            for (const string& reg : {"SEC", "CFTC"}) {
                map<string, bool>& regCache = reg == "SEC" ? hasSECCache : hasCFTCCache;
                auto& hasRegMap = reg == "SEC" ? hasSEC_ : hasCFTC_;

                if (hasRegMap[side].find(nsd) == hasRegMap[side].end()) {
                    bool hasReg = false;
                    if (regCache.find(crifRegs) != regCache.end()) {
                        hasReg = regCache.at(crifRegs);
                    } else {
                        set<string> regs = parseRegulationString(crifRegs);
                        hasReg = regs.find(reg) != regs.end();
                        regCache[crifRegs] = hasReg;
                    }
                    if (hasReg)
                        hasRegMap[side].insert(nsd);
                }
            }
        }
    }
}

void IMScheduleAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                         const std::set<std::string>& runTypes) {

    if (!analytic()->match(runTypes))
        return;

    LOG("IMScheduleAnalytic::runAnalytic called");

    analytic()->buildMarket(loader, false);

    auto imAnalytic = static_cast<IMScheduleAnalytic*>(analytic());
    QL_REQUIRE(imAnalytic, "Analytic must be of type IMScheduleAnalytic");

    
    imAnalytic->loadCrifRecords(loader);

    // Calculate IMSchedule
    LOG("Calculating Schedule IM")
    auto imSchedule = QuantLib::ext::make_shared<IMScheduleCalculator>(
        imAnalytic->crif(), inputs_->simmResultCurrency(), analytic()->market(),
        true, inputs_->enforceIMRegulations(), false, imAnalytic->hasSEC(),
        imAnalytic->hasCFTC());
    imAnalytic->setImSchedule(imSchedule);

    Real fxSpotReport = 1.0;
    if (!inputs_->simmReportingCurrency().empty()) {
        fxSpotReport = analytic()
                ->market()
                ->fxRate(inputs_->simmResultCurrency() + inputs_->simmReportingCurrency())
                ->value();
        DLOG("SIMM reporting currency is " << inputs_->simmReportingCurrency() << " with fxSpot "
                                            << fxSpotReport);
    }

    QuantLib::ext::shared_ptr<InMemoryReport> imScheduleSummaryReport = QuantLib::ext::make_shared<InMemoryReport>();
    QuantLib::ext::shared_ptr<InMemoryReport> imScheduleTradeReport = QuantLib::ext::make_shared<InMemoryReport>();

    // Populate the trade-level IM Schedule report
    LOG("Generating Schedule IM reports")
    ore::analytics::ReportWriter(inputs_->reportNaString())
            .writeIMScheduleTradeReport(imSchedule->imScheduleTradeResults(), imScheduleTradeReport,
                                        imAnalytic->hasNettingSetDetails());

    // Populate the netting set level IM Schedule report
    ore::analytics::ReportWriter(inputs_->reportNaString())
            .writeIMScheduleSummaryReport(imSchedule->finalImScheduleSummaryResults(), imScheduleSummaryReport,
                                          imAnalytic->hasNettingSetDetails(), inputs_->simmResultCurrency(),
                                          inputs_->simmReportingCurrency(), fxSpotReport);

    LOG("Schedule IM reports generated");
    MEM_LOG;

    analytic()->reports()["IM_SCHEDULE"]["im_schedule"] = imScheduleSummaryReport;
    analytic()->reports()["IM_SCHEDULE"]["im_schedule_trade"] = imScheduleTradeReport;
}

} // namespace analytics
} // namespace ore

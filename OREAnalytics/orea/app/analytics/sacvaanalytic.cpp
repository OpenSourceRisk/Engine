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

#include <orea/app/analytics/sacvaanalytic.hpp>
#include <orea/engine/standardapproachcvacalculator.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/engine/parsensitivitycubestream.hpp>
#include <orea/engine/sacvasensitivityloader.hpp>

using RFType = ore::analytics::RiskFactorKey::KeyType;

namespace ore {
namespace analytics {

void SaCvaAnalyticImpl::setUpConfigurations() {

    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->scenarioSimMarketParams();
}

void SaCvaAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                    const std::set<std::string>& runTypes) {

    LOG("SaCvaAnalyticImpl::runAnalytic called");

    if (!analytic()->match(runTypes))
        return;

    SaCvaNetSensitivities cvaSensis = inputs_->saCvaNetSensitivities();

    // Generate sensitivities here if not provided
    if (cvaSensis.size() == 0) {
        auto sacvaAnalytic = static_cast<SaCvaAnalytic*>(analytic());
        QL_REQUIRE(sacvaAnalytic, "Analytic must be of type SaCvaAnalytic");
        auto sensiAnalytic = dependentAnalytic(sensiLookupKey);
        // inputs_->setSensiThreshold(QuantLib::Null<QuantLib::Real>());
        sensiAnalytic->runAnalytic(loader, {"XVA_SENSITIVITY"});

        // Get the netting set cva par sensitivity stream from the sub analytic
        auto xvaSensiAnalytic = boost::dynamic_pointer_cast<XvaSensitivityAnalytic>(sensiAnalytic);
        XvaSensitivityAnalyticImpl::ParSensiResults parResults = xvaSensiAnalytic->getParResults();
        auto nettingSetCube = parResults.nettingParSensiCube_[XvaResults::Adjustment::CVA];
        auto pss = QuantLib::ext::make_shared<ParSensitivityCubeStream>(nettingSetCube, inputs_->baseCurrency());

        // Use the loader to map and aggregate the par sensitivity input
        SaCvaSensitivityLoader cvaLoader;
        cvaLoader.loadFromRawSensis(pss, inputs_->baseCurrency(), inputs_->counterpartyManager());
        cvaSensis = cvaLoader.netRecords();
    }

    // Report the net CVA sensis, even if we loaded them from a report
    CONSOLEW("SA-CVA: Sensitivity Report");
    auto cvaSensiReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    ReportWriter(inputs_->reportNaString()).writeSaCvaSensiReport(cvaSensis, *cvaSensiReport);
    analytic()->reports()[label()]["sacva_sensitivity"] = cvaSensiReport;
    CONSOLE("OK");

    // Create the SA-CVA result reports we want to be populated by the sacva calculator below
    auto summaryReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    auto detailReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    std::map<StandardApproachCvaCalculator::ReportType, QuantLib::ext::shared_ptr<Report>> reports;
    reports[StandardApproachCvaCalculator::ReportType::Summary] = summaryReport;
    reports[StandardApproachCvaCalculator::ReportType::Detail] = detailReport;

    // Call the SA-CVA calculator, given CVA sensitivities
    CONSOLEW("SA-CVA: Capital Reports");
    auto sacva = QuantLib::ext::make_shared<StandardApproachCvaCalculator>(
        inputs_->baseCurrency(), cvaSensis, inputs_->counterpartyManager(), reports,
        inputs_->useUnhedgedCvaSensis(), inputs_->cvaPerfectHedges());
    sacva->calculate();
    CONSOLE("OK");

    analytic()->reports()[label()]["sacva_summary"] = summaryReport;
    analytic()->reports()[label()]["sacva_detail"] = detailReport;

    LOG("SaCvaAnalyticImpl::runAnalytic done");
}

} // namespace analytics
} // namespace ore

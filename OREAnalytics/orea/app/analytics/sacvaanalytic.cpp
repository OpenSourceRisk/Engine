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

#include <orea/app/analytics/analyticfactory.hpp>
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
    analytic()->configurations().sensiScenarioData = inputs_->xvaSensiScenarioData();
}

void SaCvaAnalyticImpl::buildDependencies() {
    auto sensiAnalytic =
        AnalyticFactory::instance().build("XVA_SENSITIVITY", inputs_, analytic()->analyticsManager(), true);
    if (sensiAnalytic.second)
        addDependentAnalytic(sensiLookupKey, sensiAnalytic.second);
}

void SaCvaAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                    const std::set<std::string>& runTypes) {

    LOG("SaCvaAnalyticImpl::runAnalytic called");
    SaCvaNetSensitivities cvaSensis = inputs_->saCvaNetSensitivities();

    // Generate sensitivities here if not provided
    if (cvaSensis.size() == 0) {
	    //auto sensiAnalytic = AnalyticFactory::instance().build(analyticsLabel, inputs_).second;
        auto sensiAnalytic = dependentAnalytic(sensiLookupKey);
        sensiAnalytic->runAnalytic(loader, {"XVA_SENSITIVITY"});

	    LOG("Insert the xva sensitivity reports into the sacva analytic");
	    analytic()->reports().insert(sensiAnalytic->reports().begin(), sensiAnalytic->reports().end());

	    // Get the par cva sensitivity cube stream that we have just cached after running the sensi analytic

        auto xsai = static_cast<XvaSensitivityAnalyticImpl*>(sensiAnalytic->impl().get());
        auto pss = xsai->parCvaSensiCubeStream();
	    QL_REQUIRE(pss, "parCvaSensiCubeStream has not been populated by the xva sensitivity analytic");

	    // Use the loader to map and aggregate the par sensitivity input.
	    // We pass scenario data to cvaLoader.loadFromRawSensis(), because the loader needs the original shift types
	    // and especially shift sizes to compute the capital calulator inputs from sensitivities, in particular:
	    // - yield curve sensis have to be calculated using abolsute shifts of 1bp = 0.0001, and the input into the
	    //   capital calculator is sensitivity divided by ABSOLUTE shift size 0.0001, i.e. a partial derivative proxy;
	    // - FX rate, FX and yield vol sensis have to be calculated using RELATIVE shifts of 1% = 0.01, and the
	    //   input into the capital calculator is sensitivity divided by shift size 0.01
	    // See https://www.bis.org/basel_framework/chapter/MAR/50.htm 
        SaCvaSensitivityLoader cvaLoader;
        cvaLoader.loadFromRawSensis(pss, inputs_->baseCurrency(), analytic()->configurations().sensiScenarioData,
                                    inputs_->counterpartyManager());
        cvaSensis = cvaLoader.netRecords();

        CONSOLEW("SA-CVA: Scaled CVA Sensitivity Report");
	    auto cvaSensiReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
	    ReportWriter(inputs_->reportNaString()).writeCvaSensiReport(cvaLoader.cvaSensitivityRecords(), *cvaSensiReport);
	    analytic()->reports()[label()]["cva_sensitivities"] = cvaSensiReport;
	    CONSOLE("OK");
    }

    // Report the net CVA sensis, even if we loaded them from a report
    CONSOLEW("SA-CVA: SACVA Sensitivity Report");
    auto saCvaSensiReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    ReportWriter(inputs_->reportNaString()).writeSaCvaSensiReport(cvaSensis, *saCvaSensiReport);
    analytic()->reports()[label()]["sacva_sensitivities"] = saCvaSensiReport;
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

    analytic()->reports()[label()]["sacva"] = summaryReport;
    analytic()->reports()[label()]["sacvadetail"] = detailReport;

    LOG("SaCvaAnalyticImpl::runAnalytic done");
}

} // namespace analytics
} // namespace ore

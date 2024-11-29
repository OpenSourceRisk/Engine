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

// FIXME: temporary includes
#include <ored/utilities/marketdata.hpp>
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

    analytic()->buildPortfolio();
    analytic()->buildMarket(loader);

    QuantLib::ext::shared_ptr<SaCvaNetSensitivities> saCvaNetSensis = nullptr;

    // Either get sensis from inputs ...
    if (inputs_->saCvaNetSensitivities()) {
        saCvaNetSensis = inputs_->saCvaNetSensitivities();
    } else { // ... or build them here
        auto sacvaAnalytic = static_cast<SaCvaAnalytic*>(analytic());
        QL_REQUIRE(sacvaAnalytic, "Analytic must be of type SaCvaAnalytic");
        auto sensiAnalytic = dependentAnalytic(sensiLookupKey);
        // inputs_->setSensiThreshold(QuantLib::Null<QuantLib::Real>());
        sensiAnalytic->runAnalytic(loader, {"XVA_SENSITIVITY"});

        // Get the netting set cva par sensitivity stream from the sub analytic
        auto xvaSensiAnalytic = boost::dynamic_pointer_cast<XvaSensitivityAnalytic>(sensiAnalytic);
        XvaSensitivityAnalyticImpl::ParSensiResults parResults = xvaSensiAnalytic->getParResults();
        auto nettingCube = parResults.nettingParSensiCube_[XvaResults::Adjustment::CVA];
        auto pssNetting = QuantLib::ext::make_shared<ParSensitivityCubeStream>(nettingCube, inputs_->baseCurrency());

	// FIXME: Consolidate the following with the similar mapping procedure in inputparameters.cpp, reading from file
	
        // Scan that stream and map it to CvaSensitivityRecords
        vector<CvaSensitivityRecord> cvaSensis;
        pssNetting->reset();
        while (SensitivityRecord sr = pssNetting->next()) {
            CvaSensitivityRecord r;
            r.nettingSetId = sr.tradeId;
            string rf = prettyPrintInternalCurveName(reconstructFactor(sr.key_1, sr.desc_1));
            r.key = mapRiskFactorKeyToCvaRiskFactorKey(rf);
            r.shiftSize = sr.shift_1;
            r.currency = sr.currency;
            r.baseCva = sr.baseNpv;
            r.delta = sr.delta;
            cvaSensis.push_back(r);
        }

	// Aggregate into the net sensis we need
        SaCvaSensitivityLoader cvaLoader;
        cvaLoader.loadFromRawSensis(cvaSensis, inputs_->baseCurrency(), inputs_->counterpartyManager());
        saCvaNetSensis = QuantLib::ext::make_shared<SaCvaNetSensitivities>(cvaLoader.netRecords());
    }

    // Create the SA-CVA result reports we want to be populated by the sacva calculator below
    auto summaryReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    auto detailReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    std::map<StandardApproachCvaCalculator::ReportType, QuantLib::ext::shared_ptr<Report>> reports;
    reports[StandardApproachCvaCalculator::ReportType::Summary] = summaryReport;
    reports[StandardApproachCvaCalculator::ReportType::Detail] = detailReport;

    // FIXME: Review methodology, move settings to inputs if we keep these 
    bool unhedgedSensitivities = true;
    std::vector<std::string> perfectHedges = {"ForeignExchange|Delta", "ForeignExchange|Vega"};

    // Call the SA-CVA calculator, given CVA sensitivities
    auto sacva = QuantLib::ext::make_shared<StandardApproachCvaCalculator>(inputs_->baseCurrency(), *saCvaNetSensis,
                                                                           inputs_->counterpartyManager(), reports,
                                                                           unhedgedSensitivities, perfectHedges);
    sacva->calculate();

    analytic()->reports()[label()]["sacva_summary"] = summaryReport;
    analytic()->reports()[label()]["sacva_detail"] = detailReport;

    LOG("SaCvaAnalyticImpl::runAnalytic done");
}

} // namespace analytics
} // namespace ore

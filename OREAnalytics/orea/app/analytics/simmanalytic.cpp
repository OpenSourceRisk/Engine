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

#include <orea/app/analytics/simmanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/simm/simmcalculator.hpp>
#include <orea/simm/utilities.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

void SimmAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
}

void SimmAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                   const std::set<std::string>& runTypes) {
    CONSOLEW("SIMM: Build Market");
    analytic()->buildMarket(loader, false);
    CONSOLE("OK");

    auto simmAnalytic = static_cast<SimmAnalytic*>(analytic());
    QL_REQUIRE(simmAnalytic, "Analytic must be of type SimmAnalytic");

    LOG("Get CRIF records from CRIF loader and fill amountUSD");        
    CONSOLEW("SIMM: Load CRIF");
    simmAnalytic->loadCrifRecords(loader);
    CONSOLE("OK");

    if (analytic()->getWriteIntermediateReports()) {
        QuantLib::ext::shared_ptr<Crif> simmDataCrif = simmAnalytic->crif()->aggregate();
        QuantLib::ext::shared_ptr<InMemoryReport> simmDataReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
        ReportWriter(inputs_->reportNaString()).writeSIMMData(*simmDataCrif, simmDataReport, simmAnalytic->hasNettingSetDetails());
        analytic()->addReport(LABEL, "simm_data", simmDataReport);
        LOG("SIMM data report generated");
    }
    MEM_LOG;

    LOG("Calculating SIMM");

    // Save SIMM calibration data to output
    if (inputs_->simmCalibrationData())
        inputs_->simmCalibrationData()->toFile((inputs_->resultsPath() / "simmcalibration.xml").string());

    auto simmConfig = inputs_->getSimmConfiguration();
    simmConfig->bucketMapper()->updateFromCrif(simmAnalytic->crif());

    // Calculate SIMM
    CONSOLEW("SIMM: Calculate");
    auto simm = QuantLib::ext::make_shared<SimmCalculator>(simmAnalytic->crif(),
                                                   simmConfig,
                                                   inputs_->simmCalculationCurrencyCall(),
                                                   inputs_->simmCalculationCurrencyPost(),
                                                   inputs_->simmResultCurrency(),
                                                   analytic()->market(),
                                                   simmAnalytic->determineWinningRegulations(),
                                                   inputs_->enforceIMRegulations());
    CONSOLE("OK");    
    analytic()->addTimer("SimmCalculator", simm->timer());

    Real fxSpot = 1.0;
    if (!inputs_->simmReportingCurrency().empty()) {
        fxSpot = analytic()->market()
            ->fxRate(inputs_->simmResultCurrency() + inputs_->simmReportingCurrency())
            ->value();
        LOG("SIMM reporting currency is " << inputs_->simmReportingCurrency() << " with fxSpot " << fxSpot);
    }

    CONSOLEW("SIMM: Generate Reports");
    QuantLib::ext::shared_ptr<InMemoryReport> simmRegulationBreakdownReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    ReportWriter(inputs_->reportNaString())
        .writeSIMMReport(simm->simmResults(), simmRegulationBreakdownReport, simmAnalytic->hasNettingSetDetails(),
                         inputs_->simmResultCurrency(), inputs_->simmCalculationCurrencyCall(),
                         inputs_->simmCalculationCurrencyPost(), inputs_->simmReportingCurrency(), false, fxSpot);
    LOG("SIMM regulation breakdown report generated");
    analytic()->addReport(LABEL, "regulation_breakdown_simm", simmRegulationBreakdownReport);


    QuantLib::ext::shared_ptr<InMemoryReport> simmReport = QuantLib::ext::make_shared<InMemoryReport>(inputs_->reportBufferSize());
    ReportWriter(inputs_->reportNaString())
        .writeSIMMReport(simm->finalSimmResults(), simmReport, simmAnalytic->hasNettingSetDetails(),
                         inputs_->simmResultCurrency(), inputs_->simmCalculationCurrencyCall(),
                         inputs_->simmCalculationCurrencyPost(), inputs_->simmReportingCurrency(), fxSpot);
    analytic()->addReport(LABEL, "simm", simmReport);
    CONSOLE("OK");    
    LOG("SIMM report generated");
    MEM_LOG;
}

void SimmAnalytic::loadCrifRecords(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {
    QL_REQUIRE(inputs_, "Inputs not set");

    if (inputs_->crif() && !inputs_->crif()->empty()) {
        LOG("Loading CRIF input for SIMM");
        crif_ = inputs_->crif();
        crif_->fillAmountUsd(market());
        hasNettingSetDetails_ = crif_->hasNettingSetDetails();
    } else {
        LOG("No CRIF input provided, generating CRIF for SIMM internally");
        auto crifAnalytic = impl()->dependentAnalytic<CrifAnalytic>(crifLookupKey);
        crifAnalytic->runAnalytic(loader);
        crif_ = crifAnalytic->crif();
        if (crif_) {
            crif_->fillAmountUsd(market());
	    hasNettingSetDetails_ = crif_->hasNettingSetDetails();
	}
	else {
            QL_FAIL("Generating CRIF for SIMM failed");
        }
        LOG("Completed generating CRIF for SIMM internally");
    }
}

} // namespace analytics
} // namespace ore

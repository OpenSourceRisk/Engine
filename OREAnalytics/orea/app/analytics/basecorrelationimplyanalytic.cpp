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

#include <orea/app/analytics/basecorrelationimplyanalytic.hpp>
#include <map>

namespace ore {
namespace analytics {

/*******************************************************************
 * PRICING Analytic: NPV, CASHFLOW, CASHFLOWNPV, SENSITIVITY, STRESS
 *******************************************************************/

void BaseCorrelationImplyAnalyticImpl::setUpConfigurations() {    
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->sensiSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->sensiScenarioData();
    setGenerateAdditionalResults(true);
}

void BaseCorrelationImplyAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                                   const std::set<std::string>& runTypes) {

    Settings::instance().evaluationDate() = inputs_->asof();
    //ObservationMode::instance().setMode(inputs_->observationModel());

    QL_REQUIRE(inputs_->portfolio(), "PricingAnalytic::run: No portfolio loaded.");

    CONSOLEW("Pricing: Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    // TODO read files, for now a dummy

    CSVFileReader reader(
        "/Users/matthias.groncki/dev/oreplus3/ore/Examples/Example_79/Input/itrax_cdo_prices_20241111.csv", true);

    struct CdoPriceData {
        CdoPriceData(double attachPoint, double detachPoint, double upfront): 
        attachPoint(attachPoint), detachPoint(detachPoint), upfront(upfront){}

            double attachPoint;
        double detachPoint;
        double upfront;
        
    };

    std::map<std::string, std::vector<CdoPriceData>>
        data;

    while(reader.next())
    {    
        auto redCode = reader.get("RedCode");
        auto attach = reader.get("Attachment");
        auto detach = reader.get("Detachment");
        auto upfront = reader.get("Tranche Upfront Mid");
        //data[redCode].push_back(CdoPriceData(attach, detach, upfront));
    }

    std::cout << "Loaded Price Data" << std::endl;


    // Build Instrument

    // Search

    CONSOLEW("Pricing: Build Portfolio");
    analytic()->buildPortfolio();
    CONSOLE("OK");
}

} // namespace analytics
} // namespace ore

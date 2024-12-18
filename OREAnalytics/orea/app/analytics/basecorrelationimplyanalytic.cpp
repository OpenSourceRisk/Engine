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
#include <ored/portfolio/cdo.hpp>
#include <map>
#include <qle/instruments/syntheticcdo.hpp>

namespace ore {
namespace analytics {

class CDOCalibrationHelper {

    CDOCalibrationHelper(const std::string underlying, double attach, double detach, QuantLib::Date& indexMaturity, const Period& indexTerm, ){
        // build instrument


        // build Loss Model

        // build pricing engine


    }

    double impliedFairUpfront(double baseCorrelation) const{
        baseCorrelation_->setValue(baseCorrelation);
        double cleanNPVDetach = cdoD->cleanNPV() - cdoD->upfrontPremiumValue();
        double cleanNPVAttach = cdoA ? cdoA->cleanNPV() - cdoA->upfrontPremiumValue() : 0.0;
        return (cleanNPVDetach - cleanNPVAttach) / currentTrancheNotional_;
    }

private:
    QuantLib::ext::shared_ptr<QuantExt::SyntheticCDO> cdoD;
    QuantLib::ext::shared_ptr<QuantExt::SyntheticCDO> cdoA;
    QuantLib::ext::shared_ptr<QuantLib::SimpleQuote> baseCorrelation_;
    QuantLib::ext::shared_ptr<ore::data::Market> market_;
    double currentTrancheNotional_;

}



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
        double attachPoint;
        double detachPoint;
        double upfront;
        double spread;
        Date indexMaturity;
    };

    std::map<std::string, std::vector<CdoPriceData>>
        data;

    while(reader.next())
    {    
        auto redCode = reader.get("RedCode");
        auto attach = parseReal(reader.get("Attachment"));
        auto detach = parseReal(reader.get("Detachment"));
        auto upfront = parseReal(reader.get("Tranche Upfront Mid"));
        auto indexMaturity = parseDate(reader.get("Index Maturity"));
        auto trancheSpread = parseReal(reader.get("Tranche Spread Mid"));
        data[redCode].push_back({attach, detach, upfront, trancheSpread, indexMaturity});
    }

    std::cout << "Loaded Price Data" << std::endl;
    for(const auto& [redCode, data] : data){
        for(const auto& pd : data){
            std::cout << redCode << "," << io::iso_date(pd.indexMaturity) << "," << pd.attachPoint << ","
                      << pd.detachPoint << "," << pd.spread << "," << pd.upfront << std::endl;
            // Build Instrument
        }
    }
    




    // Search

    CONSOLEW("Pricing: Build Portfolio");
    analytic()->buildPortfolio();
    CONSOLE("OK");
}

} // namespace analytics
} // namespace ore

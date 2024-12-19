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
#include <ql/math/solvers1d/brent.hpp>

namespace ore {
namespace analytics {

class CDOCalibrationHelper {

    CDOCalibrationHelper(const Basket& basket, double attach, double detach, QuantLib::Date& indexMaturity, const Period& indexTerm, double spread, double baseCorrAttach){
        // build instrument
        auto schedule = buildSchedule();
        if (attach > 0.0 && !QuantLib::close_enough(attach, 0.0)) {
            cdoA = ext::make_shared<SyntheticCDO>(basket, Protection::Side::Buyer, schedule, 0.0, spread,
                                                  Actual365Fixed(), ModifiedFollowing);
            Handle<Quote> baseCorr(ext::make_shared<SimpleQuote>(baseCorrAttach));
            auto peA = buildPricingEngine(baseCorr, attach);
            cdoA->setPricingEngine(peA);
        }
        cdoD = ext::make_shared<SyntheticCDO>(basket, Protection::Side::Buyer, schedule, 0.0, spread, Actual365Fixed(),
                                              ModifiedFollowing);

        auto peD = buildPricingEngine(Handle<Quote>(baseCorrelation_), detach);
        cdoD->setPricingEngine(peD);
    }


    double impliedFairUpfront(double baseCorrelationD) const{
        baseCorrelation_->setValue(baseCorrelationD);
        double cleanNPVDetach = cdoD->cleanNPV() - cdoD->upfrontPremiumValue();
        double cleanNPVAttach = cdoA ? cdoA->cleanNPV() - cdoA->upfrontPremiumValue() : 0.0;
        return (cleanNPVDetach - cleanNPVAttach) / currentTrancheNotional_;
    }

private:

    Schedule buildSchedule(const Date& indexMaturity, const Period& term) const {
        Date startYear = indexMaturity - term;
        Date startDate(20, Sep, startYear.year());
        return Schedule(startDate, indexMaturity, 3 * Months, WeekendsOnly(), Unadjusted, Unadjusted,
                        DateGeneration::CDS2015, false);
    }

    ext::shared_ptr<DefaultLossModel> buildLossModel() const;
    ext::shared_ptr<PricingEngine> buildPricingEngine(const Handle<Quote>& baseCorrelation, double detach) const;




    QuantLib::ext::shared_ptr<QuantExt::SyntheticCDO> cdoD;
    QuantLib::ext::shared_ptr<QuantExt::SyntheticCDO> cdoA;
    QuantLib::ext::shared_ptr<QuantLib::SimpleQuote> baseCorrelation_;
    QuantLib::ext::shared_ptr<ore::data::Market> market_;
    double currentTrancheNotional_;
};

void BaseCorrelationImplyAnalyticImpl::setUpConfigurations() {    
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->sensiSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->sensiScenarioData();
    setGenerateAdditionalResults(true);
}


Basket buildBasketFromReferenceData(ext::shared_ptr<Market> market, ext::shared_ptr<CreditIndexReferenceDatum> refData) {
    QL_REQUIRE(refData != nullptr, "No refdata provided, can not build basket");

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
    for(auto [redCode, data] : data){

        std::sort(data.begin(), data.end(), [](auto& a, auto& b) {a.attach < b.attach});

        for(const auto& pd : data){
            std::cout << redCode << "," << io::iso_date(pd.indexMaturity) << "," << pd.attachPoint << ","
                      << pd.detachPoint << "," << pd.spread << "," << pd.upfront << std::endl;
            // Build Instrument
            auto refDataMgr = inputs_->refDataManager();
            auto crid = QuantLib::ext::dynamic_pointer_cast<CreditIndexReferenceDatum>(
                refDataMgr->getData(CreditIndexReferenceDatum::TYPE, redCode));

            CDOCalibrationHelper(redCode, )
        }
    }
    
    

    // Optimization loop
    




    // Search

    CONSOLEW("Pricing: Build Portfolio");
    analytic()->buildPortfolio();
    CONSOLE("OK");
}

} // namespace analytics
} // namespace ore

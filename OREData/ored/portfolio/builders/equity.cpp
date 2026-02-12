/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/equity.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<QuantExt::EquityCouponPricer>
EquityCouponPricerBuilder::engineImpl(const Currency& ccy, const string& equityName, const string& fxIndex) {
    return QuantLib::ext::make_shared<QuantExt::EquityCouponPricer>();
}

QuantLib::ext::shared_ptr<QuantExt::EquityCouponPricer> BlackQuantoEquityCouponPricerBuilder::engineImpl(const Currency& ccy,
                                                                       const string& equityName, const string& fxIndex) {
    parseEngineParameters();

    auto pricer = QuantLib::ext::make_shared<QuantExt::EquityCouponPricer>();

    if (applyConvexityAdjustment_) {
        string config = configuration(MarketContext::pricing);
        // Check if quanto
        Currency eqCurrency = (market_->equityCurve(equityName, config))->currency();
        if (eqCurrency != ccy) {
            string ccyPair = eqCurrency.code() + ccy.code();
            // Only apply convexity adjustment when the currency pair is in the list
            if (quantoCurrencyPairs_.empty() || quantoCurrencyPairs_.count(ccyPair) > 0) {
                Handle<BlackVolTermStructure> equityVol =
                    market_->equityVol(equityName, configuration(MarketContext::pricing));
                pricer->setEquityVolatility(equityVol);

                Handle<BlackVolTermStructure> fxVol = market_->fxVol(ccyPair, config);
                pricer->setFxVolatility(fxVol);

                std::string eqCorrelationIndexName = "EQ-" + equityName;
                std::string fxCorrelationIndexName =
                    fxIndex.empty() ? "FX-GENERIC-" + ccy.code() + "-" + eqCurrency.code() : fxIndex;
                Handle<QuantExt::CorrelationTermStructure> correlation =
                    market_->correlationCurve(eqCorrelationIndexName, fxCorrelationIndexName, config);
                pricer->setCorrelation(correlation);
            }
        }
    }
    return pricer;
}

void BlackQuantoEquityCouponPricerBuilder::parseEngineParameters() { 
    if (!parametersInitialised_) {
        applyConvexityAdjustment_ = parseBool(engineParameter("ApplyConvexityAdjustment", {}, false, "false"));
        if (applyConvexityAdjustment_) {
            vector<string> ccyPairs = parseListOfValues(engineParameter("QuantoCurrencyPairs", {}, false, ""));
            quantoCurrencyPairs_ = std::set<string>(ccyPairs.begin(), ccyPairs.end());
            // Add inverse pairs for easy look up
            std::set<std::string> inversePairs;
            for (const auto& p : quantoCurrencyPairs_) {
                QL_REQUIRE(p.size() == 6, "Currency pair must be 6 characters, got: " << p);
                std::string inversePair = p.substr(3, 3) + p.substr(0, 3);
                inversePairs.insert(inversePair);
            }
            quantoCurrencyPairs_.insert(inversePairs.begin(), inversePairs.end());
        }
        parametersInitialised_ = true;
    }
}
    
} // namespace data
} // namespace ore

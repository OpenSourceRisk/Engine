/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/creditlinkedswap.hpp>

#include <qle/pricingengines/discountingcreditlinkedswapengine.hpp>

namespace ore {
namespace data {

std::string CreditLinkedSwapEngineBuilder::keyImpl(const std::string& currency, const std::string& creditCurveId) {
    return currency + "_" + creditCurveId;
}

QuantLib::ext::shared_ptr<PricingEngine> CreditLinkedSwapEngineBuilder::engineImpl(const std::string& currency,
                                                                           const std::string& creditCurveId) {

    auto irCurve = market_->discountCurve(currency, configuration(MarketContext::pricing));
    auto creditCurve = market_->defaultCurve(creditCurveId, configuration(MarketContext::pricing))->curve();
    Handle<Quote> marketRecovery = market_->recoveryRate(creditCurveId, configuration(MarketContext::pricing));

    bool generateAdditionalResults = false;
    auto p = globalParameters_.find("GenerateAdditionalResults");
    if (p != globalParameters_.end()) {
        generateAdditionalResults = parseBool(p->second);
    }

    return QuantLib::ext::make_shared<QuantExt::DiscountingCreditLinkedSwapEngine>(
        irCurve, creditCurve, marketRecovery, parseInteger(engineParameter("TimeStepsPerYear")),
        generateAdditionalResults);
}

} // namespace data
} // namespace ore

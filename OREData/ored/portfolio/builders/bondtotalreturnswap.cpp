/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/bondtotalreturnswap.hpp>

namespace ore {
namespace data {

BondTRSEngineBuilder::BondTRSEngineBuilder(const std::string& model, const std::string& engine)
    : CachingEngineBuilder(model, engine, {"BondTRS"}) {}

QuantLib::ext::shared_ptr<PricingEngine> DiscountingBondTRSEngineBuilder::engineImpl(const string& ccy) {
    return QuantLib::ext::make_shared<QuantExt::DiscountingBondTRSEngine>(
        market_->discountCurve(ccy, configuration(MarketContext::pricing)),
        parseBool(modelParameter("TreatSecuritySpreadAsCreditSpread", {}, false, "false")),
        parseBool(modelParameter("SurvivalWeightedFundingReturnCashflows", {}, false, "false")));
}

} // namespace data
} // namespace ore

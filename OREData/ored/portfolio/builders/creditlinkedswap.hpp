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

#pragma once

#include <ored/portfolio/creditlinkedswap.hpp>

#include <ored/portfolio/builders/cachingenginebuilder.hpp>

namespace ore {
namespace data {

class CreditLinkedSwapEngineBuilder
    : public CachingPricingEngineBuilder<string, const std::string&, const std::string&> {
public:
    CreditLinkedSwapEngineBuilder()
        : CachingEngineBuilder("DiscountedCashflows", "DiscountingCreditLinkedSwapEngine", {"CreditLinkedSwap"}) {}

private:
    std::string keyImpl(const std::string& currency, const std::string& creditCurveId) override;
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const std::string& currency, const std::string& creditCurveId) override;
};

} // namespace data
} // namespace ore

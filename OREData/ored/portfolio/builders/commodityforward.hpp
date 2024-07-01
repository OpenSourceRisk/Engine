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

/*! \file portfolio/builders/commodityforward.hpp
    \brief Engine builder for commodity forward
    \ingroup builders
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <qle/pricingengines/discountingcommodityforwardengine.hpp>

namespace ore {
namespace data {

//! Engine builder for commodity forward
/*! Pricing engines are cached by currency
    \ingroup builders
 */
class CommodityForwardEngineBuilder
    : public CachingPricingEngineBuilder<std::string, const QuantLib::Currency&> {
public:
    CommodityForwardEngineBuilder()
        : CachingEngineBuilder("DiscountedCashflows", "DiscountingCommodityForwardEngine", {"CommodityForward"}) {}

protected:
    virtual std::string keyImpl(const QuantLib::Currency& ccy) override {
        return ccy.code();
    }

    virtual QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const QuantLib::Currency& ccy) override {
        return QuantLib::ext::make_shared<QuantExt::DiscountingCommodityForwardEngine>(
            market_->discountCurve(ccy.code(), configuration(MarketContext::pricing)));
    }
};

} // namespace data
} // namespace ore

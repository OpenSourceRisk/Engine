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

/*! \file portfolio/builders/commodityswap.hpp
\brief Engine builder for commodity swaps
\ingroup builders
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>

namespace ore {
namespace data {

//! Engine builder for Commodity Swaps
/*! Pricing engines are cached by currency
-
\ingroup builders
*/
class CommoditySwapEngineBuilder : public CachingPricingEngineBuilder<string, const Currency&> {
public:
    CommoditySwapEngineBuilder()
        : CachingEngineBuilder("DiscountedCashflows", "CommoditySwapEngine", {"CommoditySwap"}) {}

protected:
    virtual std::string keyImpl(const Currency& ccy) override { return ccy.code(); }

    virtual QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const Currency& ccy) override {

        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        return QuantLib::ext::make_shared<QuantLib::DiscountingSwapEngine>(yts);
    };
};

} // namespace data
} // namespace ore

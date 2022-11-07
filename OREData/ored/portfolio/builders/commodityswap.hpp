/*
Copyright (C) 2019 Quaternion Risk Management Ltd
All rights reserved.
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

    virtual boost::shared_ptr<QuantLib::PricingEngine> engineImpl(const Currency& ccy) override {

        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        return boost::make_shared<QuantLib::DiscountingSwapEngine>(yts);
    };
};

} // namespace data
} // namespace ore

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
#include <ored/utilities/marketdata.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <qle/pricingengines/discountingcurrencyswapengine.hpp>
#include <qle/pricingengines/discountingcommoditycurrencyswapengine.hpp>

namespace ore {
namespace data {

//! Engine builder for Commodity Swaps
/*! Pricing engines are cached by currency
-
\ingroup builders
*/
class CommoditySwapEngineBuilder : public CachingPricingEngineBuilder<std::string, const QuantLib::Currency&,
                                                                      const QuantLib::Currency&, const std::string&> {
public:
    CommoditySwapEngineBuilder()
        : CachingEngineBuilder("DiscountedCashflows", "CommoditySwapEngine", {"CommoditySwap"}) {}

protected:
    virtual std::string keyImpl(const Currency& ccy, const Currency& npvCcy,
                                const std::string& discountCurveName) override {
        return ccy.code() + "_" + npvCcy.code() + "_" + discountCurveName;
    }

    virtual QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    engineImpl(const Currency& ccy, const Currency& npvCcy, const std::string& discountCurveName) override {
        Handle<YieldTermStructure> yts = discountCurveName.empty()
            ? market_->discountCurve(ccy.code(), configuration(MarketContext::pricing))
            : indexOrYieldCurve(market_, discountCurveName, configuration(MarketContext::pricing));
        return QuantLib::ext::make_shared<QuantLib::DiscountingSwapEngine>(
            yts, QuantLib::ext::nullopt, Date(), Date(),
            market_->fxRate(ccy.code() + npvCcy.code(), configuration(MarketContext::pricing)));
    };
};

//! Discounted Cashflows Engine Builder for Cross Currency Commodity Swaps
class CrossCurrencyCommoditySwapEngineBuilder
    : public CachingEngineBuilder<std::string, const std::vector<QuantLib::Currency>&, const QuantLib::Currency&> {
public:
    CrossCurrencyCommoditySwapEngineBuilder()
        : CachingEngineBuilder("DiscountedCashflows", "DiscountingCrossCurrencyCommoditySwapEngine",
                               {"CrossCurrencyCommoditySwap"}) {}

protected:
    std::string keyImpl(const std::vector<Currency>& ccys, const Currency& npvCcy) {
        std::ostringstream ccyskey;
        ccyskey << npvCcy << "/";
        for (Size i = 0; i < ccys.size(); ++i)
            ccyskey << ccys[i] << "-";
        return ccyskey.str();
    }

    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const std::vector<Currency>& ccys,
                                                                  const Currency& npvCcy) {
        std::string config = configuration(MarketContext::pricing);
        std::string npvCcyCode = npvCcy.code();
        std::vector<QuantLib::Handle<QuantLib::YieldTermStructure>> discountCurves;
        std::vector<QuantLib::Handle<QuantLib::Quote>> fxQuotes;
        for (Size i = 0; i < ccys.size(); ++i) {
            std::string legCcy = ccys[i].code();
            discountCurves.push_back(market_->discountCurve(legCcy, config));
            std::string pair = legCcy + npvCcyCode;
            fxQuotes.push_back(market_->fxRate(pair, config));
        }
        return QuantLib::ext::make_shared<QuantExt::DiscountingCommodityCurrencySwapEngine>(discountCurves, fxQuotes,
                                                                                            ccys, npvCcy);
    }
};

} // namespace data
} // namespace ore

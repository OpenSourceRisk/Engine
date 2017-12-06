/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/swap.hpp
    \brief
    \ingroup builders
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <qle/pricingengines/discountingcurrencyswapengine.hpp>
#include <qle/pricingengines/discountingswapenginemulticurve.hpp>

namespace ore {
namespace data {

//! Engine Builder base class for Single Currency Swaps
/*! Pricing engines are cached by currency
    \ingroup builders
*/
class SwapEngineBuilderBase : public CachingPricingEngineBuilder<string, const Currency&> {
public:
    SwapEngineBuilderBase(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"Swap"}) {}

protected:
    virtual string keyImpl(const Currency& ccy) override { return ccy.code(); }
};

//! Engine Builder for Single Currency Swaps
/*! This builder uses QuantLib::DiscountingSwapEngine
    \ingroup builders
*/
class SwapEngineBuilder : public SwapEngineBuilderBase {
public:
    SwapEngineBuilder() : SwapEngineBuilderBase("DiscountedCashflows", "DiscountingSwapEngine") {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const Currency& ccy) override {

        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        return boost::make_shared<QuantLib::DiscountingSwapEngine>(yts);
    }
};

//! Engine Builder for Single Currency Swaps
/*! This builder uses QuantExt::DiscountingSwapEngineMultiCurve
    \ingroup builders
*/
class SwapEngineBuilderOptimised : public SwapEngineBuilderBase {
public:
    SwapEngineBuilderOptimised() : SwapEngineBuilderBase("DiscountedCashflows", "DiscountingSwapEngineOptimised") {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const Currency& ccy) override {

        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        return boost::make_shared<QuantExt::DiscountingSwapEngineMultiCurve>(yts);
    }
};

//! Engine Builder for Cross Currency Swaps
/*! Pricing engines are cached by currencies (represented as a string list)

    \ingroup builders
*/
class CrossCurrencySwapEngineBuilder
    : public CachingPricingEngineBuilder<string, const std::vector<Currency>&, const Currency&> {
public:
    CrossCurrencySwapEngineBuilder()
        : CachingEngineBuilder("DiscountedCashflows", "DiscountingCrossCurrencySwapEngine", {"CrossCurrencySwap"}) {}

protected:
    virtual string keyImpl(const std::vector<Currency>& ccys, const Currency& base) override {
        std::ostringstream ccyskey;
        ccyskey << base << "/";
        for (Size i = 0; i < ccys.size(); ++i)
            ccyskey << ccys[i] << ((i < ccys.size() - 1) ? "-" : "");
        return ccyskey.str();
    }

    virtual boost::shared_ptr<PricingEngine> engineImpl(const std::vector<Currency>& ccys,
                                                        const Currency& base) override {

        std::vector<Handle<YieldTermStructure>> discountCurves;
        std::vector<Handle<Quote>> fxQuotes;
        for (Size i = 0; i < ccys.size(); ++i) {
            discountCurves.push_back(market_->discountCurve(ccys[i].code(), configuration(MarketContext::pricing)));
            string pair = ccys[i].code() + base.code();
            fxQuotes.push_back(market_->fxSpot(pair, configuration(MarketContext::pricing)));
        }

        return boost::make_shared<QuantExt::DiscountingCurrencySwapEngine>(discountCurves, fxQuotes, ccys, base);
    }
};

} // namespace data
} // namespace ore

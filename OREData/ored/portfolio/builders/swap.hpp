/*
 Copyright (C) 2016-2022 Quaternion Risk Management Ltd
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
    \brief Engine builder for Swaps
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>

#include <qle/models/crossassetmodel.hpp>
#include <qle/pricingengines/discountingcurrencyswapengine.hpp>
#include <qle/pricingengines/discountingswapenginemulticurve.hpp>
#include <qle/pricingengines/mclgmswaptionengine.hpp>

#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

//! Engine Builder base class for Single Currency Swaps
/*! Pricing engines are cached by currency
    \ingroup builders
*/
class SwapEngineBuilderBase
    : public CachingPricingEngineBuilder<string, const Currency&, const std::string&, const std::string&> {
public:
    SwapEngineBuilderBase(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"Swap"}) {}

protected:
    virtual string keyImpl(const Currency& ccy, const std::string& discountCurve,
                           const std::string& securitySpread) override {
        return ccy.code() + discountCurve + securitySpread;
    }
};

//! Engine Builder for Single Currency Swaps
/*! This builder uses QuantLib::DiscountingSwapEngine
    \ingroup builders
*/
class SwapEngineBuilder : public SwapEngineBuilderBase {
public:
    SwapEngineBuilder() : SwapEngineBuilderBase("DiscountedCashflows", "DiscountingSwapEngine") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& ccy, const std::string& discountCurve,
                                                        const std::string& securitySpread) override {
        Handle<YieldTermStructure> yts =
            discountCurve.empty() ? market_->discountCurve(ccy.code(), configuration(MarketContext::pricing))
                                  : indexOrYieldCurve(market_, discountCurve, configuration(MarketContext::pricing));
        if (!securitySpread.empty())
            yts = Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(
                yts, market_->securitySpread(securitySpread, configuration(MarketContext::pricing))));
        return QuantLib::ext::make_shared<QuantLib::DiscountingSwapEngine>(yts);
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
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& ccy, const std::string& discountCurve,
                                                        const std::string& securitySpread) override {

        Handle<YieldTermStructure> yts =
            discountCurve.empty() ? market_->discountCurve(ccy.code(), configuration(MarketContext::pricing))
                                  : indexOrYieldCurve(market_, discountCurve, configuration(MarketContext::pricing));
        if (!securitySpread.empty())
            yts = Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(
                yts, market_->securitySpread(securitySpread, configuration(MarketContext::pricing))));
        return QuantLib::ext::make_shared<QuantExt::DiscountingSwapEngineMultiCurve>(yts);
    }
};

//! Engine Builder base class for Cross Currency Swaps
/*! Pricing engines are cached by currencies (represented as a string list)

    \ingroup builders
*/
class CrossCurrencySwapEngineBuilderBase
    : public CachingPricingEngineBuilder<string, const std::vector<Currency>&, const Currency&> {
public:
    CrossCurrencySwapEngineBuilderBase(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"CrossCurrencySwap"}) {}

protected:
    virtual string keyImpl(const std::vector<Currency>& ccys, const Currency& base) override {
        std::ostringstream ccyskey;
        ccyskey << base << "/";
        for (Size i = 0; i < ccys.size(); ++i)
            ccyskey << ccys[i] << ((i < ccys.size() - 1) ? "-" : "");
        return ccyskey.str();
    }
};

//! Discounted Cashflows Engine Builder for Cross Currency Swaps
class CrossCurrencySwapEngineBuilder : public CrossCurrencySwapEngineBuilderBase {
public:
    CrossCurrencySwapEngineBuilder()
        : CrossCurrencySwapEngineBuilderBase("DiscountedCashflows", "DiscountingCrossCurrencySwapEngine") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const std::vector<Currency>& ccys,
                                                        const Currency& base) override {

        std::vector<Handle<YieldTermStructure>> discountCurves;
        std::vector<Handle<Quote>> fxQuotes;
        std::string config = configuration(MarketContext::pricing);
        std::string baseCcyCode = base.code();
        for (Size i = 0; i < ccys.size(); ++i) {
            string ccyCode = ccys[i].code();
            discountCurves.push_back(xccyYieldCurve(market_, ccyCode, config));
            string pair = ccyCode + baseCcyCode;
            fxQuotes.push_back(market_->fxRate(pair, config));
        }

        return QuantLib::ext::make_shared<QuantExt::DiscountingCurrencySwapEngine>(discountCurves, fxQuotes, ccys, base);
    }
};

//! Implementation of SwapEngineBuilderBase using MC pricer for external cam / AMC
/*! \ingroup portfolio
 */
class CamAmcSwapEngineBuilder : public SwapEngineBuilderBase {
public:
    CamAmcSwapEngineBuilder(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                            const std::vector<Date>& simulationDates)
        : SwapEngineBuilderBase("CrossAssetModel", "AMC"), cam_(cam), simulationDates_(simulationDates) {}

protected:
    // the pricing engine depends on the ccy only, can use the caching from SwapEngineBuilderBase
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& ccy, const std::string& discountCurve,
                                                        const std::string& securitySpread) override;

private:
    QuantLib::ext::shared_ptr<PricingEngine> buildMcEngine(const QuantLib::ext::shared_ptr<QuantExt::LGM>& lgm,
                                                   const Handle<YieldTermStructure>& discountCurve,
                                                   const std::vector<Date>& simulationDates,
                                                   const std::vector<Size>& externalModelIndices);
    const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> cam_;
    const std::vector<Date> simulationDates_;
};

} // namespace data
} // namespace ore

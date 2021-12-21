/*
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

/*! \file portfolio/builders/barrieroption.hpp
    \brief Abstract engine builders for barrier options
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/vanillaoption.hpp>

#include <qle/termstructures/blackdeltautilities.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/pricingengines/barrier/analyticbarrierengine.hpp>
#include <ql/experimental/barrieroption/analyticdoublebarrierengine.hpp>
#include <ql/experimental/barrieroption/vannavolgabarrierengine.hpp>
#include <ql/experimental/barrieroption/vannavolgadoublebarrierengine.hpp>
#include <ql/experimental/exoticoptions/analyticpartialtimebarrieroptionengine.hpp>

namespace ore {
namespace data {

//! Abstract Engine Builder for Barrier Options
/*! Pricing engines are cached by asset/currency and <expiry> if engine specific

    \ingroup builders
 */
class BarrierOptionEngineBuilder
    : public CachingOptionEngineBuilder<string, const string&, const Currency&, const AssetClass&, const Date&> {
public:
    BarrierOptionEngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes,
                               const AssetClass& assetClass, const Date& expiryDate)
        : CachingOptionEngineBuilder(model, engine, tradeTypes, assetClass), expiryDate_(expiryDate) {}

    boost::shared_ptr<PricingEngine> engine(const string& assetName, const Currency& ccy, const Date& expiryDate) {
        return CachingPricingEngineBuilder<string, const string&, const Currency&, const AssetClass&,
                                           const Date&>::engine(assetName, ccy, assetClass_, expiryDate);
    }

    boost::shared_ptr<PricingEngine> engine(const Currency& ccy1, const Currency& ccy2, const Date& expiryDate) {
        return CachingPricingEngineBuilder<string, const string&, const Currency&, const AssetClass&,
                                           const Date&>::engine(ccy1.code(), ccy2, assetClass_, expiryDate);
    }

protected:
    virtual string keyImpl(const string& assetName, const Currency& ccy, const AssetClass& assetClassUnderlying,
                           const Date& expiryDate) override {
        return assetName + "/" + ccy.code() + "/" + to_string(expiryDate);
    }

    Date expiryDate_;
};

//! Abstract Engine Builder for Standard Barrier Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class StandardBarrierOptionEngineBuilder : public BarrierOptionEngineBuilder {
public:
    StandardBarrierOptionEngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes,
                                       const AssetClass& assetClass, const Date& expiryDate)
        : BarrierOptionEngineBuilder(model, engine, tradeTypes, assetClass, expiryDate) {}
};

//! Abstract Engine Builder for Standard Double Barrier Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class StandardDoubleBarrierOptionEngineBuilder : public BarrierOptionEngineBuilder {
public:
    StandardDoubleBarrierOptionEngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes,
                                             const AssetClass& assetClass, const Date& expiryDate)
        : BarrierOptionEngineBuilder(model, engine, tradeTypes, assetClass, expiryDate) {}
};

//! Abstract Engine Builder for Standard Barrier Options using Analytic Barrier Engine
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class StandardBarrierOptionAnalyticEngineBuilder : public StandardBarrierOptionEngineBuilder {
public:
    StandardBarrierOptionAnalyticEngineBuilder(const string& model, const set<string>& tradeTypes,
                                               const AssetClass& assetClass)
        : StandardBarrierOptionEngineBuilder(model, "AnalyticBarrierEngine", tradeTypes, assetClass, Date()) {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying,
                                                        const Date& expiryDate) override {
        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
            getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
        return boost::make_shared<QuantLib::AnalyticBarrierEngine>(gbsp);
    }
};

//! Abstract Engine Builder for Fx Standard Barrier Options using Vanna-Volga Barrier Engine
/*! Pricing engines are cached by asset/currency/expiry

    \ingroup builders
 */
class StandardBarrierOptionVVEngineBuilder : public StandardBarrierOptionEngineBuilder {
public:
    StandardBarrierOptionVVEngineBuilder(const string& model, const set<string>& tradeTypes,
                                         const AssetClass& assetClass, const Date& expiryDate)
        : StandardBarrierOptionEngineBuilder(model, "VannaVolgaBarrierEngine", tradeTypes, assetClass, expiryDate) {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying,
                                                        const Date& expiryDate) override {
        QL_REQUIRE(assetClassUnderlying == AssetClass::FX, "this engine is for Fx Barrier Options");

        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
            getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
        Handle<YieldTermStructure> domesticTS = gbsp->riskFreeRate();
        Handle<YieldTermStructure> foreignTS = gbsp->dividendYield();
        const string& ccyPairCode = assetName + ccy.code();
        Handle<Quote> spotFX = market_->fxSpot(ccyPairCode);

        Time ttm = gbsp->blackVolatility()->timeFromReference(expiryDate);
        Handle<DeltaVolQuote> atmVol(
            boost::make_shared<DeltaVolQuote>(Handle<Quote>(boost::make_shared<SimpleQuote>(
                                                  gbsp->blackVolatility()->blackVol(expiryDate, spotFX->value()))),
                                              DeltaVolQuote::DeltaType::Spot, ttm, DeltaVolQuote::AtmType::AtmSpot));
        Real strike25Put = QuantExt::getStrikeFromDelta(Option::Type::Put, -0.25, DeltaVolQuote::DeltaType::Spot,
                                                        spotFX->value(), domesticTS->discount(ttm),
                                                        foreignTS->discount(ttm), *gbsp->blackVolatility(), ttm);
        Real strike25Call = QuantExt::getStrikeFromDelta(Option::Type::Call, 0.25, DeltaVolQuote::DeltaType::Spot,
                                                         spotFX->value(), domesticTS->discount(ttm),
                                                         foreignTS->discount(ttm), *gbsp->blackVolatility(), ttm);
        Handle<DeltaVolQuote> vol25Put(boost::make_shared<DeltaVolQuote>(
            -0.25,
            Handle<Quote>(boost::make_shared<SimpleQuote>(gbsp->blackVolatility()->blackVol(expiryDate, strike25Put))),
            ttm, DeltaVolQuote::DeltaType::Spot));
        Handle<DeltaVolQuote> vol25Call(boost::make_shared<DeltaVolQuote>(
            0.25,
            Handle<Quote>(boost::make_shared<SimpleQuote>(gbsp->blackVolatility()->blackVol(expiryDate, strike25Call))),
            ttm, DeltaVolQuote::DeltaType::Spot));

        bool adaptVanDelta = false;  // Default false
        Real bsPriceWithSmile = 0.0; // Default 0.0

        return boost::make_shared<QuantLib::VannaVolgaBarrierEngine>(atmVol, vol25Put, vol25Call, spotFX, domesticTS,
                                                                     foreignTS, adaptVanDelta, bsPriceWithSmile);
    }
};

//! Abstract Engine Builder for Partial-time Barrier Options using Analytic Partial-time Barrier Engine
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class PartialTimeBarrierOptionAnalyticEngineBuilder : public BarrierOptionEngineBuilder {
public:
    PartialTimeBarrierOptionAnalyticEngineBuilder(const string& model, const set<string>& tradeTypes,
                                                  const AssetClass& assetClass)
        : BarrierOptionEngineBuilder(model, "AnalyticPartialTimeBarrierEngine", tradeTypes, assetClass, Date()) {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying,
                                                        const Date& expiryDate) override {
        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
            getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
        return boost::make_shared<QuantLib::AnalyticPartialTimeBarrierOptionEngine>(gbsp);
    }
};

//! Abstract Engine Builder for Standard Double Barrier Options using Analytic Double Barrier Engine
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class StandardDoubleBarrierOptionAnalyticEngineBuilder : public StandardDoubleBarrierOptionEngineBuilder {
public:
    StandardDoubleBarrierOptionAnalyticEngineBuilder(const string& model, const set<string>& tradeTypes,
                                                     const AssetClass& assetClass)
        : StandardDoubleBarrierOptionEngineBuilder(model, "AnalyticDoubleBarrierEngine", tradeTypes, assetClass,
                                                   Date()) {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying,
                                                        const Date& expiryDate) override {
        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
            getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
        // TODO: Optional series parameter? Defaults to 5.
        return boost::make_shared<QuantLib::AnalyticDoubleBarrierEngine>(gbsp /*, series*/);
    }
};

} // namespace data
} // namespace ore

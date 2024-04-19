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

/*! \file portfolio/builders/quantovanillaoption.hpp
    \brief Abstract engine builder for Quanto European Options
    \ingroup builders
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/vanillaoption.hpp>
#include <qle/termstructures/flatcorrelation.hpp>
#include <ql/pricingengines/quanto/quantoengine.hpp>

namespace ore {
namespace data{

//! Abstract Engine Builder for Quanto Vanilla Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
*/
class QuantoVanillaOptionEngineBuilder
    : public CachingOptionEngineBuilder<string, const string&, const Currency&, const Currency&, const AssetClass&,
                                        const Date&> {
public:
    QuantoVanillaOptionEngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes,
                                     const AssetClass& assetClass, const Date& expiryDate)
        : CachingOptionEngineBuilder(model, engine, tradeTypes, assetClass), expiryDate_(expiryDate) {}

    QuantLib::ext::shared_ptr<PricingEngine> engine(const string& assetName, const Currency& underlyingCcy,
                                            const Currency& payCcy, const Date& expiryDate) {
        return CachingPricingEngineBuilder<string, const string&, const Currency&, const Currency&, const AssetClass&,
                                           const Date&>::engine(assetName, underlyingCcy, payCcy, assetClass_,
                                                                expiryDate);
    }

protected:
    virtual string keyImpl(const string& assetName, const Currency& underlyingCcy, const Currency& payCcy,
                           const AssetClass& assetClassUnderlying, const Date& expiryDate) override {
        return assetName + "/" + underlyingCcy.code() + "/" + payCcy.code() + "/" + to_string(expiryDate);
    }

    Date expiryDate_;
};

//! Abstract Engine Builder for Quanto European Vanilla Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
*/
class QuantoEuropeanOptionEngineBuilder : public QuantoVanillaOptionEngineBuilder {
public:
    QuantoEuropeanOptionEngineBuilder(const string& model, const set<string>& tradeTypes, const AssetClass& assetClass)
        : QuantoVanillaOptionEngineBuilder(model, "AnalyticEuropeanEngine", tradeTypes, assetClass, Date()) {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& underlyingCcy,
                                                        const Currency& payCcy, const AssetClass& assetClassUnderlying,
                                                        const Date& expiryDate) override {
        QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess> gbsp =
            getBlackScholesProcess(assetName, underlyingCcy, assetClassUnderlying);

        Handle<YieldTermStructure> discountCurve =
            market_->discountCurve(underlyingCcy.code(), configuration(MarketContext::pricing));

        Handle<BlackVolTermStructure> fxVolatility =
            market_->fxVol(underlyingCcy.code() + payCcy.code(), configuration(MarketContext::pricing));

        std::string fxSource = modelParameters_.at("FXSource");
        std::string fxIndex = "FX-" + fxSource + "-" + underlyingCcy.code() + "-" + payCcy.code();
        std::string underlyingIndex;
        switch (assetClassUnderlying) {
            case AssetClass::EQ:
                underlyingIndex = "EQ-" + assetName;
                break;
            case AssetClass::COM:
                underlyingIndex = "COMM-" + assetName;
                break;
            default:
                QL_FAIL("Asset class " << assetClassUnderlying << " not supported for quanto vanilla option.");
        }
            
    QuantLib::Handle<QuantExt::CorrelationTermStructure> corrCurve(
        QuantLib::ext::make_shared<QuantExt::FlatCorrelation>(0, NullCalendar(), 0.0, Actual365Fixed()));
    try {
        corrCurve = market_->correlationCurve(fxIndex, underlyingIndex, configuration(MarketContext::pricing));
    } catch (...) {
        WLOG("no correlation curve for " << fxIndex << ", " << underlyingIndex
                                         << " found, fall back to zero correlation");
    }

    return QuantLib::ext::make_shared<QuantLib::QuantoEngine<VanillaOption, QuantLib::AnalyticEuropeanEngine>>(
        gbsp, discountCurve, fxVolatility,
        Handle<Quote>(
            QuantLib::ext::make_shared<QuantExt::CorrelationValue>(corrCurve, corrCurve->timeFromReference(expiryDate))));
    }
};

} // namespace data
} //namespace ore

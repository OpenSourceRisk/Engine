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
#include <ql/methods/finitedifferences/utilities/fdmquantohelper.hpp>

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

//! Abstract Engine Builder for Quanto American Vanilla Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
*/
class QuantoAmericanOptionEngineBuilder : public QuantoVanillaOptionEngineBuilder {
public:
    QuantoAmericanOptionEngineBuilder(const string& model, const set<string>& tradeTypes,
                                            const AssetClass& assetClass)
        : QuantoVanillaOptionEngineBuilder(model, "FdBlackScholesVanillaEngine", tradeTypes, assetClass, Date()) {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& underlyingCcy,
                                                                const Currency& payCcy,
                                                                const AssetClass& assetClassUnderlying,
                                                                const Date& expiryDate) override {
        // We follow the way FdBlackScholesBarrierEngine determines maturity for time grid generation
        Handle<YieldTermStructure> riskFreeRate =
            market_->discountCurve(underlyingCcy.code(), configuration(ore::data::MarketContext::pricing));

        Handle<YieldTermStructure> payCcyRate =
            market_->discountCurve(payCcy.code(), configuration(ore::data::MarketContext::pricing));

        Time expiry = riskFreeRate->dayCounter().yearFraction(riskFreeRate->referenceDate(),
                                                              std::max(riskFreeRate->referenceDate(), expiryDate));

        FdmSchemeDesc scheme = parseFdmSchemeDesc(engineParameter("Scheme"));
        Size tGrid = (Size)(parseInteger(engineParameter("TimeGridPerYear")) * expiry);
        Size xGrid = parseInteger(engineParameter("XGrid"));
        Size dampingSteps = parseInteger(engineParameter("DampingSteps"));
        bool monotoneVar = parseBool(engineParameter("EnforceMonotoneVariance", {}, false, "true"));
        Size tGridMin = parseInteger(engineParameter("TimeGridMinimumSize", {}, false, "1"));
        tGrid = std::max(tGridMin, tGrid);

        QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess> gbsp;

        if (monotoneVar) {
            // Replicate the construction of time grid in FiniteDifferenceModel::rollbackImpl
            // This time grid is required to build a BlackMonotoneVarVolTermStructure which
            // ensures monotonic variance along the time grid
            const Size totalSteps = tGrid + dampingSteps;
            std::vector<Time> timePoints(totalSteps + 1);
            Array timePointsArray(totalSteps, expiry, -expiry / totalSteps);
            timePoints[0] = 0.0;
            for (Size i = 0; i < totalSteps; i++)
                timePoints[timePoints.size() - i - 1] = timePointsArray[i];
            timePoints.insert(std::upper_bound(timePoints.begin(), timePoints.end(), 0.99 / 365), 0.99 / 365);
            gbsp = getBlackScholesProcess(assetName, underlyingCcy, assetClassUnderlying, timePoints);
        } else {
            gbsp = getBlackScholesProcess(assetName, underlyingCcy, assetClassUnderlying);
        }

        Handle<BlackVolTermStructure> fxVolatility =
            market_->fxVol(underlyingCcy.code() + payCcy.code(), configuration(MarketContext::pricing));

        std::string fxSource = engineParameter("FXSource");
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

        QuantLib::Handle<QuantExt::CorrelationTermStructure> quantoCorr(
            QuantLib::ext::make_shared<QuantExt::FlatCorrelation>(0, NullCalendar(), 0.0, Actual365Fixed()));
        try {
            quantoCorr = market_->correlationCurve(fxIndex, underlyingIndex, configuration(MarketContext::pricing));
        } catch (...) {
            WLOG("no correlation curve for " << fxIndex << ", " << underlyingIndex
                                                << " found, fall back to zero correlation");
        }

        Handle<Quote> fxSpot =
            market_->fxSpot(underlyingCcy.code() + payCcy.code(), configuration(MarketContext::pricing));
        Real fxStrike = fxSpot->value() * riskFreeRate->discount(expiryDate) / payCcyRate->discount(expiryDate);

        QuantLib::ext::shared_ptr<FdmQuantoHelper> quantoHelper;

        quantoHelper = QuantLib::ext::make_shared<FdmQuantoHelper>(
            *payCcyRate, *riskFreeRate, 
            *gbsp->blackVolatility(), 
            quantoCorr->correlation(expiryDate), 
            fxStrike);
        
        // return approach like above but fails
        // return QuantLib::ext::make_shared<QuantLib::QuantoEngine<QuantLib::VanillaOption,
        // FdBlackScholesVanillaEngine>>(
        //            gbsp, quantoHelper, tGrid, xGrid, dampingSteps, scheme);
        
        return QuantLib::ext::make_shared<QuantLib::FdBlackScholesVanillaEngine>(gbsp, quantoHelper, tGrid, xGrid,
                                                                               dampingSteps, scheme);
    }
};

} // namespace data
} //namespace ore

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

/*! \file portfolio/builders/vanillaoption.hpp
    \brief Abstract engine builders for European and American Options
    \ingroup builders
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/pricingengines/vanilla/fdblackscholesvanillaengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <qle/pricingengines/analyticcashsettledeuropeanengine.hpp>
#include <qle/pricingengines/analyticeuropeanforwardengine.hpp>
#include <qle/pricingengines/baroneadesiwhaleyengine.hpp>
#include <qle/termstructures/blackmonotonevarvoltermstructure.hpp>
#include <qle/termstructures/pricetermstructureadapter.hpp>

namespace ore {
namespace data {

template <class T, typename... Args> class CachingOptionEngineBuilder : public CachingPricingEngineBuilder<T, Args...> {
public:
    CachingOptionEngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes,
                               const AssetClass& assetClass)
        : CachingPricingEngineBuilder<T, Args...>(model, engine, tradeTypes), assetClass_(assetClass) {}

protected:
    QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess> getBlackScholesProcess(const string& assetName,
                                                                             const Currency& ccy,
                                                                             const AssetClass& assetClassUnderlying,
                                                                             const std::vector<Time>& timePoints = {},
                                                                             const bool useFxSpot = true) {

        using VVTS = QuantExt::BlackMonotoneVarVolTermStructure;
        string config = this->configuration(ore::data::MarketContext::pricing);

        if (assetClassUnderlying == AssetClass::EQ) {
            Handle<BlackVolTermStructure> vol = this->market_->equityVol(assetName, config);
            if (!timePoints.empty()) {
                vol = Handle<BlackVolTermStructure>(QuantLib::ext::make_shared<VVTS>(vol, timePoints));
                vol->enableExtrapolation();
            }
            return QuantLib::ext::make_shared<QuantLib::GeneralizedBlackScholesProcess>(
                this->market_->equitySpot(assetName, config), this->market_->equityDividendCurve(assetName, config),
                this->market_->equityForecastCurve(assetName, config), vol);

        } else if (assetClassUnderlying == AssetClass::FX) {
            const string& ccyPairCode = assetName + ccy.code();
            Handle<BlackVolTermStructure> vol = this->market_->fxVol(ccyPairCode, config);
            if (!timePoints.empty()) {
                vol = Handle<BlackVolTermStructure>(QuantLib::ext::make_shared<VVTS>(vol, timePoints));
                vol->enableExtrapolation();
            }
            if (useFxSpot) {
                return QuantLib::ext::make_shared<QuantLib::GeneralizedBlackScholesProcess>(
                    this->market_->fxSpot(ccyPairCode, config), this->market_->discountCurve(assetName, config),
                    this->market_->discountCurve(ccy.code(), config), vol);
            }
            return QuantLib::ext::make_shared<QuantLib::GeneralizedBlackScholesProcess>(
                this->market_->fxRate(ccyPairCode, config), this->market_->discountCurve(assetName, config),
                this->market_->discountCurve(ccy.code(), config), vol);
            

        } else if (assetClassUnderlying == AssetClass::COM) {

            Handle<BlackVolTermStructure> vol = this->market_->commodityVolatility(assetName, config);
            if (!timePoints.empty()) {
                vol = Handle<BlackVolTermStructure>(QuantLib::ext::make_shared<VVTS>(vol, timePoints));
                vol->enableExtrapolation();
            }

            // Create the commodity convenience yield curve for the process
            Handle<QuantExt::PriceTermStructure> priceCurve = this->market_->commodityPriceCurve(assetName, config);
            Handle<Quote> commoditySpot(QuantLib::ext::make_shared<QuantExt::DerivedPriceQuote>(priceCurve));
            Handle<YieldTermStructure> discount = this->market_->discountCurve(ccy.code(), config);
            Handle<YieldTermStructure> yield(
                QuantLib::ext::make_shared<QuantExt::PriceTermStructureAdapter>(*priceCurve, *discount));
            yield->enableExtrapolation();

            return QuantLib::ext::make_shared<QuantLib::GeneralizedBlackScholesProcess>(commoditySpot, yield, discount, vol);

        } else {
            QL_FAIL("Asset class of " << (int)assetClassUnderlying << " not recognized.");
        }
    }
    AssetClass assetClass_;
};

//! Abstract Engine Builder for Vanilla Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class VanillaOptionEngineBuilder
    : public CachingOptionEngineBuilder<string, const string&, const Currency&, const AssetClass&, const Date&, const bool> {
public:
    VanillaOptionEngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes,
                               const AssetClass& assetClass, const Date& expiryDate)
        : CachingOptionEngineBuilder(model, engine, tradeTypes, assetClass), expiryDate_(expiryDate) {}

    QuantLib::ext::shared_ptr<PricingEngine> engine(const string& assetName, const Currency& ccy, const Date& expiryDate, const bool useFxSpot = true) {
        return CachingPricingEngineBuilder<string, const string&, const Currency&, const AssetClass&,
                                           const Date&, const bool>::engine(assetName, ccy, assetClass_, expiryDate, useFxSpot);
    }

    QuantLib::ext::shared_ptr<PricingEngine> engine(const Currency& ccy1, const Currency& ccy2, const Date& expiryDate,
                                            const bool useFxSpot = true) {
        return CachingPricingEngineBuilder<string, const string&, const Currency&, const AssetClass&,
                                           const Date&, const bool>::engine(ccy1.code(), ccy2, assetClass_, expiryDate, useFxSpot);
    }

protected:
    virtual string keyImpl(const string& assetName, const Currency& ccy, const AssetClass& assetClassUnderlying,
                           const Date& expiryDate, const bool useFxSpot) override {
        return assetName + "/" + ccy.code() + "/" + to_string(expiryDate);
    }

    Date expiryDate_;
};

//! Abstract Engine Builder for European Vanilla Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class EuropeanOptionEngineBuilder : public VanillaOptionEngineBuilder {
public:
    EuropeanOptionEngineBuilder(const string& model, const set<string>& tradeTypes, const AssetClass& assetClass)
        : VanillaOptionEngineBuilder(model, "AnalyticEuropeanEngine", tradeTypes, assetClass, Date()) {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying,
                                                        const Date& expiryDate, const bool useFxSpot) override {
        QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess> gbsp =
            getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
        Handle<YieldTermStructure> discountCurve =
            market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        return QuantLib::ext::make_shared<QuantLib::AnalyticEuropeanEngine>(gbsp, discountCurve);
    }
};

//! Abstract Engine Builder for European Vanilla Forward Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class EuropeanForwardOptionEngineBuilder : public VanillaOptionEngineBuilder {
public:
    EuropeanForwardOptionEngineBuilder(const string& model, const set<string>& tradeTypes, const AssetClass& assetClass)
        : VanillaOptionEngineBuilder(model, "AnalyticEuropeanForwardEngine", tradeTypes, assetClass, Date()) {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const bool useFxSpot) override {
        QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess> gbsp =
            getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
        Handle<YieldTermStructure> discountCurve =
            market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        return QuantLib::ext::make_shared<QuantExt::AnalyticEuropeanForwardEngine>(gbsp, discountCurve);
    }
};

/*! European cash-settled option engine builder
    \ingroup builders
 */
class EuropeanCSOptionEngineBuilder : public VanillaOptionEngineBuilder {
public:
    EuropeanCSOptionEngineBuilder(const string& model, const set<string>& tradeTypes, const AssetClass& assetClass)
        : VanillaOptionEngineBuilder(model, "AnalyticCashSettledEuropeanEngine", tradeTypes, assetClass, Date()) {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const bool useFxSpot) override {
        QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess> gbsp =
            getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
        Handle<YieldTermStructure> discountCurve =
            market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        return QuantLib::ext::make_shared<QuantExt::AnalyticCashSettledEuropeanEngine>(gbsp, discountCurve);
    }
};

//! Abstract Engine Builder for American Vanilla Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class AmericanOptionEngineBuilder : public VanillaOptionEngineBuilder {
public:
    AmericanOptionEngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes,
                                const AssetClass& assetClass, const Date& expiryDate)
        : VanillaOptionEngineBuilder(model, engine, tradeTypes, assetClass, expiryDate) {}
};

//! Abstract Engine Builder for American Vanilla Options using Finite Difference Method
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class AmericanOptionFDEngineBuilder : public AmericanOptionEngineBuilder {
public:
    AmericanOptionFDEngineBuilder(const string& model, const set<string>& tradeTypes, const AssetClass& assetClass,
                                  const Date& expiryDate)
        : AmericanOptionEngineBuilder(model, "FdBlackScholesVanillaEngine", tradeTypes, assetClass, expiryDate) {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClass, const Date& expiryDate,
                                                        const bool useFxSpot) override {
        // We follow the way FdBlackScholesBarrierEngine determines maturity for time grid generation
        Handle<YieldTermStructure> riskFreeRate =
            market_->discountCurve(ccy.code(), configuration(ore::data::MarketContext::pricing));
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
            gbsp = getBlackScholesProcess(assetName, ccy, assetClass, timePoints);
        } else {
            gbsp = getBlackScholesProcess(assetName, ccy, assetClass);
        }
        return QuantLib::ext::make_shared<FdBlackScholesVanillaEngine>(gbsp, tGrid, xGrid, dampingSteps, scheme);
    }
};

//! Abstract Engine Builder for American Vanilla Options using Barone Adesi Whaley Approximation
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class AmericanOptionBAWEngineBuilder : public AmericanOptionEngineBuilder {
public:
    AmericanOptionBAWEngineBuilder(const string& model, const set<string>& tradeTypes, const AssetClass& assetClass)
        : AmericanOptionEngineBuilder(model, "BaroneAdesiWhaleyApproximationEngine", tradeTypes, assetClass, Date()) {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClass, const Date& expiryDate,
                                                        const bool useFxSpot) override {
        QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess> gbsp = getBlackScholesProcess(assetName, ccy, assetClass);
        return QuantLib::ext::make_shared<QuantExt::BaroneAdesiWhaleyApproximationEngine>(gbsp);
    }
};

} // namespace data
} // namespace ore

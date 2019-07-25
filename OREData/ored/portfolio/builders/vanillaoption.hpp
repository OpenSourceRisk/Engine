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
    \brief
    \ingroup builders
*/

#pragma once

#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/pricingengines/vanilla/fdblackscholesvanillaengine.hpp>
#include <qle/pricingengines/baroneadesiwhaleyengine.hpp>
#include <qle/termstructures/blackmonotonevarvoltermstructure.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/version.hpp>

namespace ore {
namespace data {

//! Abstract Engine Builder for Vanilla Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class VanillaOptionEngineBuilder : public CachingPricingEngineBuilder<string, const string&, const Currency&,
                                                                              const AssetClass&, const Date&> {
public:
    VanillaOptionEngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes,
        const AssetClass& assetClass, const Date& expiryDate)
        : CachingEngineBuilder(model, engine, tradeTypes),
          assetClass_(assetClass), expiryDate_(expiryDate) {}

    boost::shared_ptr<PricingEngine> engine(const string& assetName, const Currency& ccy, const Date& expiryDate) {
        return CachingPricingEngineBuilder<string, const string&, const Currency&,
            const AssetClass&, const Date&>::engine(assetName, ccy, assetClass_, expiryDate);
    }

    boost::shared_ptr<PricingEngine> engine(const Currency& ccy1, const Currency& ccy2, const Date& expiryDate) {
        return CachingPricingEngineBuilder<string, const string&, const Currency&,
            const AssetClass&, const Date&>::engine(ccy1.code(), ccy2, assetClass_, expiryDate);
    }

protected:
    virtual string keyImpl(const string& assetName, const Currency& ccy,
                           const AssetClass& assetClassUnderlying, const Date& expiryDate) override {
        return assetName + "/" + ccy.code() + "/" + boost::lexical_cast<string>(expiryDate);
    }

    const Time getBucketedExpiry(const Time& expiry) {
        QL_REQUIRE(expiry >=0, "expiry cannot be negative");
        if(expiry <= 2) {
            // Round to next quarter of year for expiry within 2 years
            return UpRounding(0).operator()(expiry * 4) / 4;
        } else {
            // Round to next year for expiry from 2 years
            return UpRounding(0).operator()(expiry);
        }
    }

    boost::shared_ptr<GeneralizedBlackScholesProcess> getBlackScholesProcess(const string& assetName,
                                                                             const Currency& ccy,
                                                                             const AssetClass& assetClassUnderlying,
                                                                             const std::vector<Time>& timePoints = {}) {
        if (assetClassUnderlying == AssetClass::EQ) {
            Handle<BlackVolTermStructure> vol = market_->equityVol(assetName, configuration(ore::data::MarketContext::pricing));
            if(!timePoints.empty()) {
                vol = Handle<BlackVolTermStructure>(
                    boost::make_shared<QuantExt::BlackMonotoneVarVolTermStructure>(vol, timePoints));
                vol->enableExtrapolation();
            }
            return boost::make_shared<GeneralizedBlackScholesProcess>(
                market_->equitySpot(assetName, configuration(ore::data::MarketContext::pricing)),
                market_->equityDividendCurve(assetName, configuration(ore::data::MarketContext::pricing)),
                market_->equityForecastCurve(assetName, configuration(ore::data::MarketContext::pricing)),
                vol);

        } else if (assetClassUnderlying == AssetClass::FX) {
            const string& ccyPairCode = assetName + ccy.code();
            Handle<BlackVolTermStructure> vol = market_->fxVol(ccyPairCode, configuration(ore::data::MarketContext::pricing));
            if(!timePoints.empty()) {
                vol = Handle<BlackVolTermStructure>(
                    boost::make_shared<QuantExt::BlackMonotoneVarVolTermStructure>(vol, timePoints));
                vol->enableExtrapolation();
            }
            return boost::make_shared<GeneralizedBlackScholesProcess>(
                market_->fxSpot(ccyPairCode, configuration(ore::data::MarketContext::pricing)),
                market_->discountCurve(assetName, configuration(ore::data::MarketContext::pricing)),
                market_->discountCurve(ccy.code(), configuration(ore::data::MarketContext::pricing)),
                vol);
        } else {
            QL_FAIL("Asset class of " << (int)assetClassUnderlying << " not recognized.");
        }
    }
    AssetClass assetClass_;
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
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
        const AssetClass& assetClassUnderlying, const Date& expiryDate) override {
        string key = keyImpl(assetName, ccy, assetClassUnderlying, expiryDate);
        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp = getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
        Handle<YieldTermStructure> discountCurve =
            market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        return boost::make_shared<QuantLib::AnalyticEuropeanEngine>(gbsp, discountCurve);
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
    AmericanOptionFDEngineBuilder(const string& model, const set<string>& tradeTypes,
                                  const AssetClass& assetClass, const Date& expiryDate)
        : AmericanOptionEngineBuilder(model, "FdBlackScholesVanillaEngine", tradeTypes, assetClass, expiryDate) {}
protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClass, const Date& expiryDate) override {
        Time expiry = Actual365Fixed().yearFraction(Settings::instance().evaluationDate(), expiryDate);
        FdmSchemeDesc scheme = parseFdmSchemeDesc(engineParameter("Scheme"));
        Size tGrid = ore::data::parseInteger(engineParameter("TimeGridPerYear")) * getBucketedExpiry(expiry);
        Size xGrid = ore::data::parseInteger(engineParameter("XGrid"));
        Size dampingSteps = ore::data::parseInteger(engineParameter("DampingSteps"));
        bool monotoneVar = ore::data::parseBool(engineParameter("EnforceMonotoneVariance", "", false, "true"));

        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp;

        if(monotoneVar) {
            // Replicate the construction of time grid in FiniteDifferenceModel::rollbackImpl
            // This time grid is required to build a BlackMonotoneVarVolTermStructure which
            // ensures monotonic variance along the time grid
            std::vector<Time> timePoints(tGrid + 2);
            Array timePointsArray(tGrid, expiry, -expiry / tGrid);
            timePoints[0] = 0.0;
            timePoints[1] = 0.99 / 365;
            for(Size i = 0; i < tGrid; i++)
                timePoints[timePoints.size() - i - 1] = timePointsArray[i];
            gbsp = getBlackScholesProcess(assetName, ccy, assetClass, timePoints);
        } else {
            gbsp = getBlackScholesProcess(assetName, ccy, assetClass);
        }
        return boost::make_shared<FdBlackScholesVanillaEngine>(gbsp, tGrid, xGrid,
                                                               dampingSteps, scheme);
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
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClass, const Date& expiryDate) override {
            boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp = getBlackScholesProcess(assetName, ccy, assetClass);
            return boost::make_shared<QuantExt::BaroneAdesiWhaleyApproximationEngine>(gbsp);
    }
};

} // namespace data
} // namespace ore

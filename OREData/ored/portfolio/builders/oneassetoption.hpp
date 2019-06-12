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

/*! \file portfolio/builders/oneassetoption.hpp
    \brief
    \ingroup builders
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/pricingengines/vanilla/fdblackscholesvanillaengine.hpp>
#include <ql/pricingengines/vanilla/baroneadesiwhaleyengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/version.hpp>

namespace ore {
namespace data {

//! Abstract Engine Builder for One Asset Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class OneAssetOptionEngineBuilder : public CachingPricingEngineBuilder<string, const string&, const Currency&, const AssetClass&> {
public:
    OneAssetOptionEngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes, const AssetClass& assetClass)
        : CachingEngineBuilder(model, engine, tradeTypes), assetClass_(assetClass) {}

    boost::shared_ptr<PricingEngine> engine(const string& assetName, const Currency& ccy) {
        return CachingPricingEngineBuilder<string, const string&, const Currency&, const AssetClass&>::engine(assetName, ccy, assetClass_);
    }

    boost::shared_ptr<PricingEngine> engine(const Currency& ccy1, const Currency& ccy2) {
        return CachingPricingEngineBuilder<string, const string&, const Currency&, const AssetClass&>::engine(ccy1.code(), ccy2, assetClass_);
    }

protected:
    virtual string keyImpl(const string& assetName, const Currency& ccy, const AssetClass& assetClassUnderlying) override {
        return assetName + "/" + ccy.code();
    }

    boost::shared_ptr<GeneralizedBlackScholesProcess> getBlackScholesProcess(const string& assetName,
                                                                             const Currency& ccy,
                                                                             const AssetClass& assetClassUnderlying) {
        if (assetClassUnderlying == AssetClass::EQ) {
            return boost::make_shared<GeneralizedBlackScholesProcess>(
                market_->equitySpot(assetName, configuration(ore::data::MarketContext::pricing)),
                market_->equityDividendCurve(assetName, configuration(ore::data::MarketContext::pricing)),
                market_->equityForecastCurve(assetName, configuration(ore::data::MarketContext::pricing)),
                market_->equityVol(assetName, configuration(ore::data::MarketContext::pricing)));

        } else if (assetClassUnderlying == AssetClass::FX) {
            const string& ccyPairCode = assetName + ccy.code();
            return boost::make_shared<GeneralizedBlackScholesProcess>(
                market_->fxSpot(ccyPairCode, configuration(ore::data::MarketContext::pricing)),
                market_->discountCurve(assetName, configuration(ore::data::MarketContext::pricing)),
                market_->discountCurve(ccy.code(), configuration(ore::data::MarketContext::pricing)),
                market_->fxVol(ccyPairCode, configuration(ore::data::MarketContext::pricing)));
        } else {
            QL_FAIL("Asset class of " << (int)assetClassUnderlying << " not recognized.");
        }
    }
    AssetClass assetClass_;
};

//! Abstract Engine Builder for European One Asset Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class OneAssetEuropeanOptionEngineBuilder : public OneAssetOptionEngineBuilder {
public:
    OneAssetEuropeanOptionEngineBuilder(const string& model, const set<string>& tradeTypes, const AssetClass& assetClass)
        : OneAssetOptionEngineBuilder(model, "AnalyticEuropeanEngine", tradeTypes, assetClass) {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy, const AssetClass& assetClassUnderlying) override {
        string key = keyImpl(assetName, ccy, assetClassUnderlying);
        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp = getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
        Handle<YieldTermStructure> discountCurve =
            market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        return boost::make_shared<QuantLib::AnalyticEuropeanEngine>(gbsp, discountCurve);
    }
};

//! Abstract Engine Builder for American One Asset Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class OneAssetAmericanOptionEngineBuilder : public OneAssetOptionEngineBuilder {
public:
    OneAssetAmericanOptionEngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes, const AssetClass& assetClass)
        : OneAssetOptionEngineBuilder(model, engine, tradeTypes, assetClass) {}
};

//! Abstract Engine Builder for American One Asset Options using Finite Difference Method
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class OneAssetAmericanOptionFDEngineBuilder : public OneAssetAmericanOptionEngineBuilder {
public:
    OneAssetAmericanOptionFDEngineBuilder(const string& model, const set<string>& tradeTypes, const AssetClass& assetClass)
        : OneAssetAmericanOptionEngineBuilder(model, "FdBlackScholesVanillaEngine", tradeTypes, assetClass) {}
protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClass) override {
        FdmSchemeDesc scheme = parseFdmSchemeDesc(engineParameter("Scheme"));
        Size tGrid = ore::data::parseInteger(engineParameter("TimeGrid"));
        Size xGrid = ore::data::parseInteger(engineParameter("XGrid"));
        Size dampingSteps = ore::data::parseInteger(engineParameter("DampingSteps"));

        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp = getBlackScholesProcess(assetName, ccy, assetClass);
        return boost::make_shared<FdBlackScholesVanillaEngine>(gbsp, tGrid, xGrid,
                                                               dampingSteps, scheme);
    }
};

//! Abstract Engine Builder for American One Asset Options using Barone Adesi Whaley Approximation
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class OneAssetAmericanOptionBaroneAdesiWhaleyEngineBuilder : public OneAssetAmericanOptionEngineBuilder {
public:
    OneAssetAmericanOptionBaroneAdesiWhaleyEngineBuilder(const string& model, const set<string>& tradeTypes, const AssetClass& assetClass)
        : OneAssetAmericanOptionEngineBuilder(model, "BaroneAdesiWhaleyApproximationEngine", tradeTypes, assetClass) {}
protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClass) override {
            boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp = getBlackScholesProcess(assetName, ccy, assetClass);
            return boost::make_shared<BaroneAdesiWhaleyApproximationEngine>(gbsp);
        }
};

} // namespace data
} // namespace ore

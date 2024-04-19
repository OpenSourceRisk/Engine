/*
 Copyright (C) 20 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/builders/deltagammaengines.hpp
    \brief Additional builders for engines that return deltas, vegas, gammas, cross-gammas
    \ingroup portfolio
*/

#pragma once

#include <qle/pricingengines/analyticeuropeanenginedeltagamma.hpp>
#include <qle/pricingengines/discountingcurrencyswapenginedeltagamma.hpp>
#include <qle/pricingengines/discountingfxforwardenginedeltagamma.hpp>
#include <qle/pricingengines/discountingswapenginedeltagamma.hpp>

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/vanillaoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/utilities/log.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;
using namespace ore::data;

//! Engine Builder for Single Currency Swaps
/*! This builder uses DiscountingSwapEngineDeltaGamma
    \ingroup portfolio
*/
class SwapEngineBuilderDeltaGamma : public SwapEngineBuilderBase {
public:
    SwapEngineBuilderDeltaGamma() : SwapEngineBuilderBase("DiscountedCashflows", "DiscountingSwapEngineDeltaGamma") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& ccy, const std::string& discountCurve,
                                                        const std::string& securitySpread) override {

        std::vector<Time> bucketTimes = parseListOfValues<Time>(engineParameter("BucketTimes"), &parseReal);
        bool computeDelta = parseBool(engineParameter("ComputeDelta"));
        bool computeGamma = parseBool(engineParameter("ComputeGamma"));
        bool computeBPS = false; // parseBool(engineParameter("ComputeBPS"));

        Handle<YieldTermStructure> yts = discountCurve.empty()
                                             ? market_->discountCurve(ccy.code(), configuration(MarketContext::pricing))
                                             : indexOrYieldCurve(market_, discountCurve, configuration(MarketContext::pricing));
        if (!securitySpread.empty())
            yts = Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(
                yts, market_->securitySpread(securitySpread, configuration(MarketContext::pricing))));
        return QuantLib::ext::make_shared<DiscountingSwapEngineDeltaGamma>(yts, bucketTimes, computeDelta,
                                                                             computeGamma, computeBPS);
    }
};

//! Engine Builder for Cross Currency Swaps
/*! This builder uses DiscountingCurrencySwapEngineDeltaGamma
 */
class CurrencySwapEngineBuilderDeltaGamma : public CrossCurrencySwapEngineBuilderBase {
public:
    CurrencySwapEngineBuilderDeltaGamma()
        : CrossCurrencySwapEngineBuilderBase("DiscountedCashflows", "DiscountingCrossCurrencySwapEngineDeltaGamma") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const std::vector<Currency>& ccys,
                                                        const Currency& base) override {

        std::vector<Time> bucketTimes = parseListOfValues<Time>(engineParameter("BucketTimes"), &parseReal);
        bool computeDelta = parseBool(engineParameter("ComputeDelta"));
        bool computeGamma = parseBool(engineParameter("ComputeGamma"));
        bool applySimmExemptions = parseBool(engineParameter("ApplySimmExemptions", {}, false, "false"));
        bool linearInZero = parseBool(engineParameter("LinearInZero", {}, false, "true"));
        std::vector<Handle<YieldTermStructure>> discountCurves;
        std::vector<Handle<Quote>> fxQuotes;
        for (Size i = 0; i < ccys.size(); ++i) {
            discountCurves.push_back(xccyYieldCurve(market_, ccys[i].code(), configuration(MarketContext::pricing)));
            string pair = ccys[i].code() + base.code();
            fxQuotes.push_back(market_->fxRate(pair, configuration(MarketContext::pricing)));
        }
        return QuantLib::ext::make_shared<DiscountingCurrencySwapEngineDeltaGamma>(
            discountCurves, fxQuotes, ccys, base, bucketTimes, computeDelta, computeGamma, linearInZero,
            applySimmExemptions);
    }
};

//! Engine Builder for European Options with delta/gamma extension
/*!
    \ingroup builders
 */
class EuropeanOptionEngineBuilderDeltaGamma : public VanillaOptionEngineBuilder {
public:
    EuropeanOptionEngineBuilderDeltaGamma(const string& model, const set<string>& tradeTypes,
                                          const AssetClass& assetClass)
        : VanillaOptionEngineBuilder(model, "AnalyticEuropeanEngineDeltaGamma", tradeTypes, assetClass, Date()) {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying,
                                                        const Date& expiryDate, const bool useFxSpot) override {
        std::vector<Time> bucketTimesDeltaGamma =
            parseListOfValues<Time>(engineParameter("BucketTimesDeltaGamma"), &parseReal);
        std::vector<Time> bucketTimesVega = parseListOfValues<Time>(engineParameter("BucketTimesVega"), &parseReal);
        bool computeDeltaVega = parseBool(engineParameter("ComputeDeltaVega"));
        bool computeGamma = parseBool(engineParameter("ComputeGamma"));

        string pair = assetName + ccy.code();
        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
            getBlackScholesProcess(assetName, ccy, assetClassUnderlying);

        return QuantLib::ext::make_shared<AnalyticEuropeanEngineDeltaGamma>(
            gbsp, bucketTimesDeltaGamma, bucketTimesVega, computeDeltaVega, computeGamma);
    }
};

//! Engine Builder for European FX Options with analytical sensitivities
/*!
  \ingroup builders
 */
class FxEuropeanOptionEngineBuilderDeltaGamma : public EuropeanOptionEngineBuilderDeltaGamma {
public:
    FxEuropeanOptionEngineBuilderDeltaGamma()
        : EuropeanOptionEngineBuilderDeltaGamma("GarmanKohlhagen", {"FxOption"}, AssetClass::FX) {}
};

//! Engine Builder for European Equity Options with analytical sensitivities
/*! Pricing engines are cached by asset/currency
    \ingroup builders
 */
class EquityEuropeanOptionEngineBuilderDeltaGamma : public EuropeanOptionEngineBuilderDeltaGamma {
public:
    EquityEuropeanOptionEngineBuilderDeltaGamma()
        : EuropeanOptionEngineBuilderDeltaGamma("BlackScholesMerton", {"EquityOption"}, AssetClass::EQ) {}
};

//! Engine Builder for FX Forwards
/*! This builder uses DiscountingFxForwardEngineDeltaGamma
 */
class FxForwardEngineBuilderDeltaGamma : public FxForwardEngineBuilderBase {
public:
    FxForwardEngineBuilderDeltaGamma()
        : FxForwardEngineBuilderBase("DiscountedCashflows", "DiscountingFxForwardEngineDeltaGamma") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& forCcy, const Currency& domCcy) override {

        std::vector<Time> bucketTimes = parseListOfValues<Time>(engineParameter("BucketTimes"), &parseReal);
        bool computeDelta = parseBool(engineParameter("ComputeDelta"));
        bool computeGamma = parseBool(engineParameter("ComputeGamma"));
        bool linearInZero = parseBool(engineParameter("LinearInZero", {}, false, "True")); // FIXME: Add to pricing engine parameters?
        bool applySimmExemptions = parseBool(engineParameter("ApplySimmExemptions", {}, false, "false"));

        string pair = keyImpl(forCcy, domCcy);
        Handle<YieldTermStructure> domCcyCurve =
            market_->discountCurve(domCcy.code(), configuration(MarketContext::pricing));
        Handle<YieldTermStructure> forCcyCurve =
            market_->discountCurve(forCcy.code(), configuration(MarketContext::pricing));
        Handle<Quote> fx = market_->fxRate(pair, configuration(MarketContext::pricing));

        return QuantLib::ext::make_shared<DiscountingFxForwardEngineDeltaGamma>(
            domCcy, domCcyCurve, forCcy, forCcyCurve, fx, bucketTimes, computeDelta, computeGamma, linearInZero,
            boost::none, Date(), Date(), applySimmExemptions);
    }
};

} // namespace data
} // namespace ore

/*
  Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/builders/varianceswap.hpp
    \brief variance swap engine builder
    \ingroup builders
*/

#pragma once

#include <ored/utilities/parsers.hpp>
#include <qle/pricingengines/varianceswapgeneralreplicationengine.hpp>
#include <qle/pricingengines/volatilityfromvarianceswapengine.hpp>
#include <qle/termstructures/pricetermstructureadapter.hpp>

#include <string>

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace ore {
namespace data {
using namespace std;
using namespace QuantLib;

//! Engine Builder for Variance Swaps
/*! Pricing engines are cached by equity/currency

    \ingroup builders
 */
class VarSwapEngineBuilder : public ore::data::CachingPricingEngineBuilder<string, const string&, const Currency&,
                                                                           const AssetClass&, const MomentType&> {
public:
    VarSwapEngineBuilder()
        : CachingEngineBuilder("BlackScholesMerton", "ReplicatingVarianceSwapEngine",
                               {"EquityVarianceSwap", "FxVarianceSwap", "CommodityVarianceSwap"}) {}

protected:
    virtual string keyImpl(const string& underlyingName, const Currency& ccy, const AssetClass& assetClassUnderlying,
                           const MomentType& momentType) override {
        return underlyingName + "/" + ccy.code() + "/" +
               (momentType == MomentType::Variance ? "Variance" : "Volatility");
    }

    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& underlyingName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying,
                                                        const MomentType& momentType) override {
        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp;
        QuantLib::ext::shared_ptr<Index> index;
        if (assetClassUnderlying == AssetClass::EQ) {
            gbsp = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
                market_->equitySpot(underlyingName, configuration(ore::data::MarketContext::pricing)),
                market_->equityDividendCurve(underlyingName, configuration(ore::data::MarketContext::pricing)),
                market_->equityForecastCurve(underlyingName, configuration(ore::data::MarketContext::pricing)),
                market_->equityVol(underlyingName, configuration(ore::data::MarketContext::pricing)));
            index = market_->equityCurve(underlyingName).currentLink();
        } else if (assetClassUnderlying == AssetClass::FX) {
            const auto fxIndex = parseFxIndex("FX-" + underlyingName);
            const string& sourceCurrencyCode = fxIndex->sourceCurrency().code();
            const string& targetCurrencyCode = fxIndex->targetCurrency().code();
            const string& ccyPairCode = sourceCurrencyCode + targetCurrencyCode;
            gbsp = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
                market_->fxSpot(ccyPairCode, configuration(ore::data::MarketContext::pricing)),
                market_->discountCurve(targetCurrencyCode, configuration(ore::data::MarketContext::pricing)),
                market_->discountCurve(sourceCurrencyCode, configuration(ore::data::MarketContext::pricing)),
                market_->fxVol(ccyPairCode, configuration(ore::data::MarketContext::pricing)));

            index = buildFxIndex("FX-" + underlyingName, targetCurrencyCode, sourceCurrencyCode, market_,
                                 configuration(MarketContext::pricing));
         } else if (assetClassUnderlying == AssetClass::COM) {
            Handle<BlackVolTermStructure> vol = market_->commodityVolatility(underlyingName, configuration(ore::data::MarketContext::pricing));
            Handle<QuantExt::PriceTermStructure> priceCurve = market_->commodityPriceCurve(underlyingName, configuration(ore::data::MarketContext::pricing));
            Handle<Quote> commoditySpot(QuantLib::ext::make_shared<QuantExt::DerivedPriceQuote>(priceCurve));
            Handle<YieldTermStructure> discount = market_->discountCurve(ccy.code(), configuration(ore::data::MarketContext::pricing));
            Handle<YieldTermStructure> yield(QuantLib::ext::make_shared<QuantExt::PriceTermStructureAdapter>(*priceCurve, *discount));
            yield->enableExtrapolation();
            gbsp = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(commoditySpot, yield, discount, vol);
            index = market_->commodityIndex(underlyingName).currentLink();
        } else {
            QL_FAIL("Asset class of " + underlyingName + " not recognized.");
        }

        QuantExt::GeneralisedReplicatingVarianceSwapEngine::VarSwapSettings settings;

        std::string schemeStr = engineParameter("Scheme", {}, false, "GaussLobatto");
        std::string boundsStr = engineParameter("Bounds", {}, false, "PriceThreshold");

        if (schemeStr == "GaussLobatto")
            settings.scheme = QuantExt::GeneralisedReplicatingVarianceSwapEngine::VarSwapSettings::Scheme::GaussLobatto;
        else if (schemeStr == "Segment")
            settings.scheme = QuantExt::GeneralisedReplicatingVarianceSwapEngine::VarSwapSettings::Scheme::Segment;
        else {
            QL_FAIL("invalid var swap pricing engine parameter Scheme (" << schemeStr
                                                                         << "), expected GaussLobatto, Segment");
        }

        if (boundsStr == "Fixed")
            settings.bounds = QuantExt::GeneralisedReplicatingVarianceSwapEngine::VarSwapSettings::Bounds::Fixed;
        else if (boundsStr == "PriceThreshold")
            settings.bounds = QuantExt::GeneralisedReplicatingVarianceSwapEngine::VarSwapSettings::Bounds::PriceThreshold;
        else {
            QL_FAIL("invalid var swap pricing engine parameter Bounds (" << boundsStr
                                                                         << "), expected Fixed, PriceThreshold");
        }

        settings.accuracy = parseReal(engineParameter("Accuracy", {}, false, "1E-5"));
        settings.maxIterations = parseInteger(engineParameter("MaxIterations", {}, false, "1000"));
        settings.steps = parseInteger(engineParameter("Steps", {}, false, "100"));
        settings.priceThreshold = parseReal(engineParameter("PriceThreshold", {}, false, "1E-10"));
        settings.maxPriceThresholdSteps = parseInteger(engineParameter("MaxPriceThresholdSteps", {}, false, "100"));
        settings.priceThresholdStep = parseReal(engineParameter("PriceThresholdStep", {}, false, "0.1"));
        settings.fixedMinStdDevs = parseReal(engineParameter("FixedMinStdDevs", {}, false, "-5.0"));
        settings.fixedMaxStdDevs = parseReal(engineParameter("FixedMaxStdDevs", {}, false, "5.0"));

        bool staticTodaysSpot = false;
        if (auto it = globalParameters_.find("RunType");
            it != globalParameters_.end() && it->second != "Exposure") {
            staticTodaysSpot = parseBool(modelParameter("StaticTodaysSpot", {}, false, "false"));
        }

        if (momentType == MomentType::Variance)
            return QuantLib::ext::make_shared<QuantExt::GeneralisedReplicatingVarianceSwapEngine>(
                index, gbsp, market_->discountCurve(ccy.code(), configuration(ore::data::MarketContext::pricing)),
                settings, staticTodaysSpot);
        else
            return QuantLib::ext::make_shared<QuantExt::VolatilityFromVarianceSwapEngine>(
                index, gbsp, market_->discountCurve(ccy.code(), configuration(ore::data::MarketContext::pricing)),
                settings, staticTodaysSpot);
    }
};

} // namespace data
} // namespace ore

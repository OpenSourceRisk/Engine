/*
  Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/commodityapo.hpp
\brief Engine builder for commodity average price options
\ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/commodityapomodelbuilder.hpp>

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>

#include <qle/pricingengines/commodityapoengine.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

//! Engine builder base class for Commodity Average Price Options
/*! Pricing engines are cached by currency and underlying name
-
\ingroup builders
*/
class CommodityApoBaseEngineBuilder
    : public CachingPricingEngineBuilder<string, const Currency&, const string&, const string&,
                                         const QuantLib::ext::shared_ptr<QuantExt::CommodityAveragePriceOption>&> {
public:
    CommodityApoBaseEngineBuilder(const std::string& model, const std::string& engine,
                                  const std::set<std::string>& tradeTypes)
        : CachingEngineBuilder(model, engine, tradeTypes) {}

protected:
    std::string keyImpl(const Currency& ccy, const string& name, const string& id,
                        const QuantLib::ext::shared_ptr<QuantExt::CommodityAveragePriceOption>&) override {
        return id;
    }
};

//! Analytical Engine builder for Commodity Average Price Options
/*! Pricing engines are cached by currency and underlying name
-
\ingroup builders
*/
class CommodityApoAnalyticalEngineBuilder : public CommodityApoBaseEngineBuilder {
public:
    CommodityApoAnalyticalEngineBuilder()
        : CommodityApoBaseEngineBuilder("Black", "AnalyticalApproximation", {"CommodityAveragePriceOption"}) {}

protected:
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    engineImpl(const Currency& ccy, const string& name, const string& id,
               const QuantLib::ext::shared_ptr<QuantExt::CommodityAveragePriceOption>& apo) override {

        Handle<QuantLib::BlackVolTermStructure> vol =
            market_->commodityVolatility(name, configuration(MarketContext::pricing));
        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));

        Real beta = 0;
        auto param = engineParameters_.find("beta");
        if (param != engineParameters_.end())
            beta = parseReal(param->second);
        else {
            ALOG("Missing engine parameter 'beta' for " << model() << " " << EngineBuilder::engine()
                                                        << ", using default value " << beta);
        }

        bool dontCalibrate = false;
        if (auto g = globalParameters_.find("Calibrate"); g != globalParameters_.end()) {
            dontCalibrate = !parseBool(g->second);
        }

        auto modelBuilder = QuantLib::ext::make_shared<CommodityApoModelBuilder>(yts, vol, apo, dontCalibrate);
        modelBuilders_.insert(std::make_pair(id, modelBuilder));

        return QuantLib::ext::make_shared<QuantExt::CommodityAveragePriceOptionAnalyticalEngine>(yts, modelBuilder->model(),
                                                                                         beta);
    };
};

//! Monte Carlo Engine builder for Commodity Average Price Options
/*! Pricing engines are cached by currency and underlying name
-
\ingroup builders
*/
class CommodityApoMonteCarloEngineBuilder : public CommodityApoBaseEngineBuilder {
public:
    CommodityApoMonteCarloEngineBuilder()
        : CommodityApoBaseEngineBuilder("Black", "MonteCarlo",
                                        {"CommodityAveragePriceOption", "CommodityAveragePriceBarrierOption"}) {}

protected:
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    engineImpl(const Currency& ccy, const string& name, const string& id,
               const QuantLib::ext::shared_ptr<QuantExt::CommodityAveragePriceOption>& apo) override {

        Handle<QuantLib::BlackVolTermStructure> vol =
            market_->commodityVolatility(name, configuration(MarketContext::pricing));
        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));

        Size samples = 10000;
        auto param = engineParameters_.find("samples");
        if (param != engineParameters_.end())
            samples = parseInteger(param->second);
        else {
            ALOG("Missing engine parameter 'samples' for " << model() << " " << EngineBuilder::engine()
                                                           << ", using default value " << samples);
        }

        Real beta = 0;
        param = engineParameters_.find("beta");
        if (param != engineParameters_.end())
            beta = parseReal(param->second);
        else {
            ALOG("Missing engine parameter 'beta' for " << model() << " " << EngineBuilder::engine()
                                                        << ", using default value " << beta);
        }

        bool dontCalibrate = false;
        if (auto g = globalParameters_.find("Calibrate"); g != globalParameters_.end()) {
            dontCalibrate = !parseBool(g->second);
        }

        auto modelBuilder = QuantLib::ext::make_shared<CommodityApoModelBuilder>(yts, vol, apo, dontCalibrate);
        modelBuilders_.insert(std::make_pair(id, modelBuilder));

        return QuantLib::ext::make_shared<QuantExt::CommodityAveragePriceOptionMonteCarloEngine>(yts, modelBuilder->model(),
                                                                                         samples, beta);
    };
};

} // namespace data
} // namespace ore

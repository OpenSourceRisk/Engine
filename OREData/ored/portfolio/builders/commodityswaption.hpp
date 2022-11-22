/*
Copyright (C) 2019 Quaternion Risk Management Ltd
All rights reserved.
*/

/*! \file portfolio/builders/commodityswaption.hpp
\brief Engine builder for commodity swaptions
\ingroup builders
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <qle/pricingengines/commodityswaptionengine.hpp>

namespace ore {
namespace data {

//! Engine builder for Commodity Swaptions
/*! Pricing engines are cached by currency and underlying name
\ingroup builders
*/
class CommoditySwaptionEngineBuilder : public CachingPricingEngineBuilder<string, const Currency&, const string&> {
public:
    explicit CommoditySwaptionEngineBuilder(const std::string& engine)
        : CachingEngineBuilder("Black", engine, {"CommoditySwaption"}) {}

protected:
    std::string keyImpl(const Currency& ccy, const string& name) override { return ccy.code() + ":" + name; }
};

//! Analytical Approximation Engine builder for Commodity Swaptions
/*! Pricing engines are cached by currency and underlying name
\ingroup builders
*/
class CommoditySwaptionAnalyticalEngineBuilder : public CommoditySwaptionEngineBuilder {
public:
    CommoditySwaptionAnalyticalEngineBuilder() : CommoditySwaptionEngineBuilder("AnalyticalApproximation") {}

protected:
    boost::shared_ptr<QuantLib::PricingEngine> engineImpl(const Currency& ccy, const string& name) override {
        Handle<QuantLib::BlackVolTermStructure> vol =
            market_->commodityVolatility(name, configuration(MarketContext::pricing));
        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        Real beta = parseReal(engineParameter("beta"));
        QL_REQUIRE(beta >= 0.0, "CommoditySwaptionAnalyticalEngineBuilder: beta must be non-negative");
        return boost::make_shared<QuantExt::CommoditySwaptionEngine>(yts, vol, beta);
    }
};
//! Monte Carlo Engine builder for Commodity Swaptions
/*! Pricing engines are cached by currency and underlying name
\ingroup builders
*/
class CommoditySwaptionMonteCarloEngineBuilder : public CommoditySwaptionEngineBuilder {
public:
    CommoditySwaptionMonteCarloEngineBuilder() : CommoditySwaptionEngineBuilder("MonteCarlo") {}

protected:
    boost::shared_ptr<QuantLib::PricingEngine> engineImpl(const Currency& ccy, const string& name) override {
        Handle<QuantLib::BlackVolTermStructure> vol =
            market_->commodityVolatility(name, configuration(MarketContext::pricing));
        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        Real beta = parseReal(engineParameter("beta"));
        QL_REQUIRE(beta >= 0.0, "CommoditySwaptionAnalyticalEngineBuilder: beta must be non-negative");
        Size samples = parseInteger(engineParameter("samples"));
        long seed = parseInteger(engineParameter("seed"));
        return boost::make_shared<QuantExt::CommoditySwaptionMonteCarloEngine>(yts, vol, samples, beta, seed);
    }

private:
};

} // namespace data
} // namespace ore

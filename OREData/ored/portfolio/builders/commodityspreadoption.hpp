/*
Copyright (C) 2022 Quaternion Risk Management Ltd
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
#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <qle/pricingengines/commodityspreadoptionengine.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/termstructures/flatcorrelation.hpp>

namespace ore::data{
//! Base Engine builder for Commodity Spread Options
/*! Pricing engines are cached by currency
-
\ingroup builders
*/

class CommoditySpreadOptionBaseEngineBuilder : public CachingPricingEngineBuilder<std::string, const Currency&, QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> const&,
                                                                                  QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> const&, std::string const&> {
public:
    CommoditySpreadOptionBaseEngineBuilder(const std::string& model, const std::string& engine,
                                           const std::set<std::string>& tradeTypes)
        : CachingEngineBuilder(model, engine, tradeTypes) {}
protected:
    std::string keyImpl(const Currency&, QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> const&, QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> const&, std::string const& id) override {
        return id;
    }

};


//! Analytical Engine builder for Commodity Spread Options
/*! Pricing engines are cached by currency
-
\ingroup builders

*/
class CommoditySpreadOptionEngineBuilder : public CommoditySpreadOptionBaseEngineBuilder{
public:
    CommoditySpreadOptionEngineBuilder()
        : CommoditySpreadOptionBaseEngineBuilder("BlackScholes", "CommoditySpreadOptionEngine", {"CommoditySpreadOption"}){}
protected:

    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const Currency& ccy, QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> const& longIndex, QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> const& shortIndex, string const& id) override{
        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        Handle<QuantLib::BlackVolTermStructure> volLong =
            market_->commodityVolatility(longIndex->underlyingName(), configuration(MarketContext::pricing));
        Handle<QuantLib::BlackVolTermStructure> volShort =
            market_->commodityVolatility(shortIndex->underlyingName(), configuration(MarketContext::pricing));
        Real beta = 0;
        Handle<QuantExt::CorrelationTermStructure> rho{nullptr};
        auto param = engineParameters_.find("beta");
        if (param != engineParameters_.end())
            beta = parseReal(param->second);
        else {
            ALOG("Missing engine parameter 'beta' for " << model() << " " << EngineBuilder::engine()
                                                        << ", using default value " << beta);
        }
        if(longIndex->underlyingName() == shortIndex->underlyingName()){ // calendar spread option
            rho = Handle<QuantExt::CorrelationTermStructure>(QuantLib::ext::make_shared<QuantExt::FlatCorrelation>(0,QuantLib::NullCalendar(), 1.0, QuantLib::Actual365Fixed()));
        }else {
            rho = market_->correlationCurve("COMM-"+longIndex->underlyingName(), "COMM-"+shortIndex->underlyingName(),
                                                  configuration(MarketContext::pricing));

        }
        return QuantLib::ext::make_shared<QuantExt::CommoditySpreadOptionAnalyticalEngine>(yts, volLong, volShort, rho, beta);
    }
};
}

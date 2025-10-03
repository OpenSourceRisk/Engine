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
#include <ored/utilities/marketdata.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/pricingengines/commodityspreadoptionengine.hpp>
#include <qle/termstructures/flatcorrelation.hpp>

namespace ore::data {
//! Base Engine builder for Commodity Spread Options
/*! Pricing engines are cached by currency
-
\ingroup builders
*/

class CommoditySpreadOptionBaseEngineBuilder
    : public CachingPricingEngineBuilder<
          std::string, const Currency&, const std::string&, QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> const&,
          QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> const&, std::string const&> {
public:
    CommoditySpreadOptionBaseEngineBuilder(const std::string& model, const std::string& engine,
                                           const std::set<std::string>& tradeTypes)
        : CachingEngineBuilder(model, engine, tradeTypes) {}

protected:
    std::string keyImpl(const Currency& ccy, const std::string& discountCurveName,
                        QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> const& comm1,
                        QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> const& comm2,
                        std::string const& id) override {
        return id + "/" + ccy.code() + "/" + discountCurveName + "/" + comm1->name() + "/" + comm2->name();
    }
};

//! Analytical Engine builder for Commodity Spread Options
/*! Pricing engines are cached by currency
-
\ingroup builders

*/
class CommoditySpreadOptionEngineBuilder : public CommoditySpreadOptionBaseEngineBuilder {
public:
    CommoditySpreadOptionEngineBuilder()
        : CommoditySpreadOptionBaseEngineBuilder("BlackScholes", "CommoditySpreadOptionEngine",
                                                 {"CommoditySpreadOption"}) {}

protected:
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    engineImpl(const Currency& ccy, const std::string& discountCurveName,
               QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> const& longIndex,
               QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> const& shortIndex, string const& id) override {
        Handle<YieldTermStructure> yts =
            discountCurveName.empty()
                ? market_->discountCurve(ccy.code(), configuration(MarketContext::pricing))
                : indexOrYieldCurve(market_, discountCurveName, configuration(MarketContext::pricing));
        Handle<QuantLib::BlackVolTermStructure> volLong =
            market_->commodityVolatility(longIndex->underlyingName(), configuration(MarketContext::pricing));
        Handle<QuantLib::BlackVolTermStructure> volShort =
            market_->commodityVolatility(shortIndex->underlyingName(), configuration(MarketContext::pricing));
        Real beta = 0;
        Handle<QuantExt::CorrelationTermStructure> rho{nullptr};
        bool calendarSpread = longIndex->underlyingName() == shortIndex->underlyingName();

        beta = parseReal(
            engineParameter("beta", {longIndex->underlyingName(), shortIndex->underlyingName()}, false, "0.0"));

        std::vector<string> volatilityQualifiers;

        if (calendarSpread) {
            volatilityQualifiers.push_back(longIndex->underlyingName());
        } else {
            std::string pair = longIndex->underlyingName() + "_" + shortIndex->underlyingName();
            std::string pairReverse = shortIndex->underlyingName() + "_" + longIndex->underlyingName();
            volatilityQualifiers.push_back(pair);
            volatilityQualifiers.push_back(pairReverse);
        }

        QuantLib::DiffusionModelType volType = QuantLib::DiffusionModelType::AsInputVolatilityType;
        Real displacement = 0.0;
        auto volTypeStr = this->modelParameter("Volatility", volatilityQualifiers, false, "AsInputVolatilityType");
        boost::to_lower(volTypeStr);
        auto displacementStr = this->modelParameter("Displacement", volatilityQualifiers, false, "0.0");
        if (volTypeStr == "lognormal") {
            volType = QuantLib::DiffusionModelType::Black;
        } else if (volTypeStr == "shiftedlognormal") {
            volType = QuantLib::DiffusionModelType::Black;
            displacement = parseReal(displacementStr);
        } else if (volTypeStr == "normal")
            volType = QuantLib::DiffusionModelType::Bachelier;
        else
            volType = QuantLib::DiffusionModelType::AsInputVolatilityType;

        if (calendarSpread) {
            rho = Handle<QuantExt::CorrelationTermStructure>(QuantLib::ext::make_shared<QuantExt::FlatCorrelation>(
                0, QuantLib::NullCalendar(), 1.0, QuantLib::Actual365Fixed()));
        } else {
            rho =
                market_->correlationCurve("COMM-" + longIndex->underlyingName(), "COMM-" + shortIndex->underlyingName(),
                                          configuration(MarketContext::pricing));
        }
        return QuantLib::ext::make_shared<QuantExt::CommoditySpreadOptionAnalyticalEngine>(yts, volLong, volShort, rho,
                                                                                           beta, volType, displacement);
    }
};
} // namespace ore::data

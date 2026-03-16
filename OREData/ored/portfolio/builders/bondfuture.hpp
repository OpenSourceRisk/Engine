/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/forwardbond.hpp
    \brief Engine builder for forward bonds
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/pricingengines/discountingbondfutureengine.hpp>

namespace ore {
namespace data {

class BondFutureEngineBuilder
    : public CachingPricingEngineBuilder<string, const string&, const string&, const double> {
protected:
    BondFutureEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"BondFuture"}) {}

    virtual string keyImpl(const string& id, const string& ccy, const double conversionFactor) override {
        return ccy + "_" + std::to_string(conversionFactor);
    }
};

class DiscountingBondFutureEngineBuilder : public BondFutureEngineBuilder {
public:
    DiscountingBondFutureEngineBuilder()
        : BondFutureEngineBuilder("DiscountedCashflows", "DiscountingBondFutureEngine") {}

protected:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& id, const string& ccy,
                                                        const double conversionFactor) override {
        return QuantLib::ext::make_shared<QuantExt::DiscountingBondFutureEngine>(
            market_->discountCurve(ccy, configuration(MarketContext::pricing)),
            Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(conversionFactor)));
    }
};

} // namespace data
} // namespace ore

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

#pragma once

#include <qle/pricingengines/discountingbondtrsengine.hpp>

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {
using namespace std;

class BondTRSEngineBuilder : public CachingPricingEngineBuilder<string, const string&> {
protected:
    BondTRSEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"BondTRS"}) {}

    virtual string keyImpl(const string& ccy) override { return ccy; }
};

class DiscountingBondTRSEngineBuilder : public BondTRSEngineBuilder {
public:
    DiscountingBondTRSEngineBuilder() : BondTRSEngineBuilder("DiscountedCashflows", "DiscountingBondTRSEngine") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& ccy) override {
        Handle<YieldTermStructure> oisCurve = market_->discountCurve(ccy, configuration(MarketContext::pricing));
        return QuantLib::ext::make_shared<QuantExt::DiscountingBondTRSEngine>(oisCurve);
    }
};

} // namespace data
} // namespace ore

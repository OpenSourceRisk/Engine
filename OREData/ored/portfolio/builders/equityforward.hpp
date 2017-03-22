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

/*! \file portfolio/builders/equityforward.hpp
    \brief
    \ingroup portfolio
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <qle/pricingengines/discountingequityforwardengine.hpp>

namespace ore {
namespace data {

//! Engine Builder for European Equity Options
/*! Pricing engines are cached by equity/currency

    \ingroup portfolio
 */
class EquityForwardEngineBuilder : public CachingPricingEngineBuilder<string, const string&, const Currency&> {
public:
    EquityForwardEngineBuilder() : CachingEngineBuilder("DiscountedCashflows", "DiscountingEquityForwardEngine") {}

protected:
    virtual string keyImpl(const string& equityName, const Currency& ccy) override {
        return equityName + "/" + ccy.code();
    }

    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& equityName, const Currency& ccy) override {
        return boost::make_shared<QuantExt::DiscountingEquityForwardEngine>(
            market_->discountCurve(ccy.code(), configuration(MarketContext::pricing)),
            market_->equityDividendCurve(equityName, configuration(MarketContext::pricing)),
            market_->equitySpot(equityName, configuration(MarketContext::pricing)),
            market_->discountCurve(ccy.code(), configuration(MarketContext::pricing)));
    }
};

} // namespace data
} // namespace ore

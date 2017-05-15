/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/creditdefaultswap.hpp
\brief
\ingroup portfolio
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>

#include <qle/pricingengines/midpointcdsengine.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

//! Engine Builder base class for Credit Default Swaps
/*! Pricing engines are cached by creditCurveId
    \ingroup portfolio
*/

class CreditDefaultSwapEngineBuilder : public CachingPricingEngineBuilder<string, const Currency&, const string&> {
protected:
    CreditDefaultSwapEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine) {}

    virtual string keyImpl(const Currency&, const string& creditCurveId) override { return creditCurveId; }
};

//! Midpoint Engine Builder class for CreditDefaultSwaps
/*! This class creates a MidPointCdsEngine
    \ingroup portfolio
*/

class MidPointCdsEngineBuilder : public CreditDefaultSwapEngineBuilder {
public:
    MidPointCdsEngineBuilder() : CreditDefaultSwapEngineBuilder("DiscountedCashflows", "MidPointCdsEngine") {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const Currency& ccy, const string& creditCurveId) override {

        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        Handle<DefaultProbabilityTermStructure> dpts =
            market_->defaultCurve(creditCurveId, configuration(MarketContext::pricing));
        Handle<Quote> recovery = market_->recoveryRate(creditCurveId, configuration(MarketContext::pricing));

        return boost::make_shared<QuantExt::MidPointCdsEngine>(dpts, recovery->value(), yts);
    }
};

} // namespace data
} // namespace ore

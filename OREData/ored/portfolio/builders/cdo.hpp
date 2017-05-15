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

/*! \file portfolio/builders/cdo.hpp
    \brief Mid point CDO engines cached by currency
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/utilities/log.hpp>
#include <qle/pricingengines/midpointcdoengine.hpp>
#include <boost/make_shared.hpp>

namespace ore {
namespace data {

//! Engine Builder base class for CDOs
/*! Pricing engines are cached by currency
    \ingroup portfolio
*/
class CdoEngineBuilder : public CachingPricingEngineBuilder<string, const Currency&> {
public:
    CdoEngineBuilder(const std::string& model, const std::string& engine) : CachingEngineBuilder(model, engine) {}

protected:
    virtual string keyImpl(const Currency& ccy) override { return ccy.code(); }
};

class GaussCopulaBucketingCdoEngineBuilder : public CdoEngineBuilder {
public:
    GaussCopulaBucketingCdoEngineBuilder() : CdoEngineBuilder("GaussCopula", "Bucketing") {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const Currency& ccy) override {
        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        return boost::shared_ptr<PricingEngine>(new QuantExt::MidPointCDOEngine(yts));
    }
};

} // namespace data
} // namespace ore

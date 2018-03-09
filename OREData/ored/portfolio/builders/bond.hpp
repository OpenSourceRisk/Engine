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

/*! \file portfolio/builders/bond.hpp
\brief
\ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>

#include <qle/pricingengines/discountingriskybondengine.hpp>

#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

//! Engine Builder base class for Bonds
/*! Pricing engines are cached by security id
\ingroup builders
*/

class BondEngineBuilder
    : public CachingPricingEngineBuilder<string, const Currency&, const string&, const string&, const string&> {
protected:
    BondEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"Bond"}) {}

    virtual string keyImpl(const Currency&, const string&, const string& securityId, const string&) override {
        return securityId;
    }
};

//! Discounting Engine Builder class for Bonds
/*! This class creates a DiscountingRiskyBondEngine
\ingroup builders
*/

class BondDiscountingEngineBuilder : public BondEngineBuilder {
public:
    BondDiscountingEngineBuilder() : BondEngineBuilder("DiscountedCashflows", "DiscountingRiskyBondEngine") {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const Currency& ccy, const string& creditCurveId,
                                                        const string& securityId,
                                                        const string& referenceCurveId) override {

        string tsperiodStr = engineParameters_.at("TimestepPeriod");
        Period tsperiod = parsePeriod(tsperiodStr);
        Handle<YieldTermStructure> yts = market_->yieldCurve(referenceCurveId, configuration(MarketContext::pricing));
        Handle<DefaultProbabilityTermStructure> dpts =
            market_->defaultCurve(creditCurveId, configuration(MarketContext::pricing));
        Handle<Quote> recovery;
        try {
            // try security recovery first
            recovery = market_->recoveryRate(securityId, configuration(MarketContext::pricing));
        } catch (...) {
            // otherwise fall back on curve recovery
            ALOG("security specific recovery rate not found for security ID "
                 << securityId << ", falling back on the recovery rate for credit curve Id " << creditCurveId);
            recovery = market_->recoveryRate(creditCurveId, configuration(MarketContext::pricing));
        }
        Handle<Quote> spread = market_->securitySpread(securityId, configuration(MarketContext::pricing));

        return boost::make_shared<QuantExt::DiscountingRiskyBondEngine>(yts, dpts, recovery, spread, tsperiod);
    }
};

} // namespace data
} // namespace ore

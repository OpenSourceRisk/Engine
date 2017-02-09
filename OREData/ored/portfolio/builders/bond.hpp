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
\ingroup portfolio
*/

#pragma once

#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/utilities/log.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>
#include <boost/make_shared.hpp>

namespace ore {
namespace data {

//! Engine Builder base class for Bonds
/*! Pricing engines are cached by currency
    \ingroup portfolio
*/
class BondEngineBuilder : public CachingPricingEngineBuilder<string, const Currency&, const string&, const string&> {
public:
	BondEngineBuilder() : CachingEngineBuilder("DiscountedCashflows", "DiscountingRiskyBondEngine") {}

protected:
	virtual string keyImpl(const Currency& ccy, const string&, const string&) override { return ccy.code(); }

	virtual boost::shared_ptr<PricingEngine> engineImpl(const Currency& ccy, const string& issuerId, const string& securityId) override {
        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        Handle<DefaultProbabilityTermStructure> dpts = market_->defaultCurve(issuerId, configuration(MarketContext::pricing));
        Handle<Quote> recovery = market_->recoveryRate(issuerId, configuration(MarketContext::pricing));
        Handle<Quote> spread = market_->securitySpread(securityId, configuration(MarketContext::pricing));

        return boost::make_shared<QuantExt::DiscountingRiskyBondEngine>(yts, dpts, recovery, spread);
	}
};

} // namespace data
} // namespace ore

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

/*! \file ored/portfolio/builders/bondoption.hpp
\brief Engine builder for bond option
\ingroup builders
*/

#pragma once

#include <qle/pricingengines/blackbondoptionengine.hpp>

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/marketdata.hpp>

#include <qle/termstructures/pricetermstructureadapter.hpp>

#include <ql/processes/blackscholesprocess.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

//! Engine builder for bond option
/*! Pricing engines are cached by currency, creditCurve, securityId and referenceCurve
-
\ingroup builders
*/
class BondOptionEngineBuilder
    : public CachingPricingEngineBuilder<string, const string&, const Currency&, const string&, const bool,
                                         const string&, const string&, const string&> {
public:
    BondOptionEngineBuilder() : CachingEngineBuilder("Black", "BlackBondOptionEngine", {"BondOption"}) {}

protected:
    virtual std::string keyImpl(const string& id, const Currency& ccy, const string& creditCurveId,
                                const bool hasCreditRisk, const string& securityId, const string& referenceCurveId,
                                const string& volatilityCurveId) override {
        // id is _not_ part of the key
        return ccy.code() + "_" + creditCurveId + "_" + (hasCreditRisk ? "1_" : "0_") + securityId + "_" +
               referenceCurveId + "_" + volatilityCurveId + "_" + "BondOption";
    }

    virtual QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    engineImpl(const string& id, const Currency& ccy, const string& creditCurveId, const bool hasCreditRisk,
               const string& securityId, const string& referenceCurveId, const string& volatilityCurveId) override {

        Handle<YieldTermStructure> discountCurve =
            market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        QL_REQUIRE(!volatilityCurveId.empty(), "BondOptionEngineBuilder: volatility curve ID for trade id '"
                                                   << id << "', security id '" << securityId << "' not given");
        Handle<QuantLib::SwaptionVolatilityStructure> yieldVola =
            market_->yieldVol(volatilityCurveId, configuration(MarketContext::pricing));
        Handle<YieldTermStructure> yts = market_->yieldCurve(referenceCurveId, configuration(MarketContext::pricing));
        Handle<DefaultProbabilityTermStructure> dpts;
        // credit curve may not always be used. If credit curve ID is empty proceed without it
        if (!creditCurveId.empty())
            dpts =
                securitySpecificCreditCurve(market_, securityId, creditCurveId, configuration(MarketContext::pricing))
                    ->curve();
        Handle<Quote> recovery;
        try {
            // try security recovery first
            recovery = market_->recoveryRate(securityId, configuration(MarketContext::pricing));
        } catch (...) {
            // otherwise fall back on curve recovery
            ALOG("security specific recovery rate not found for security ID "
                 << securityId << ", falling back on the recovery rate for credit curve Id " << creditCurveId);
            if (!creditCurveId.empty())
                recovery = market_->recoveryRate(creditCurveId, configuration(MarketContext::pricing));
        }
        Handle<Quote> spread;
        try {
            // spread is optional, pass empty handle to engine if not given (will be treated as 0 spread there)
            spread = market_->securitySpread(securityId, configuration(MarketContext::pricing));
        } catch (...) {
        }

        if (!hasCreditRisk) {
            dpts = Handle<DefaultProbabilityTermStructure>();
        }

        return QuantLib::ext::make_shared<QuantExt::BlackBondOptionEngine>(
            discountCurve, yieldVola, yts, dpts, recovery, spread, parsePeriod(engineParameter("TimestepPeriod")));
    };
};
} // namespace data
} // namespace ore

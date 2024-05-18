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

/*! \file portfolio/builders/forwardbond.hpp
    \brief Engine builder for forward bonds
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/marketdata.hpp>

#include <qle/instruments/impliedbondspread.hpp>
#include <qle/pricingengines/discountingforwardbondengine.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>

#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

class fwdBondEngineBuilder
    : public CachingPricingEngineBuilder<string, const string&, const Currency&, const string&, const bool,
                                         const string&, const string&, const string&> {
protected:
    fwdBondEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"ForwardBond"}) {}

    virtual string keyImpl(const string& id, const Currency& ccy, const string& creditCurveId, const bool hasCreditRisk,
                           const string& securityId, const string& referenceCurveId,
                           const string& incomeCurveId) override {

        // id is _not_ part of the key
        std::string returnString = ccy.code() + "_" + creditCurveId + "_" + (hasCreditRisk ? "1_" : "0_") + securityId +
                                   "_" + referenceCurveId + "_" + incomeCurveId;

        return returnString;
    }
};

class DiscountingForwardBondEngineBuilder : public fwdBondEngineBuilder {
public:
    DiscountingForwardBondEngineBuilder()
        : fwdBondEngineBuilder("DiscountedCashflows", "DiscountingForwardBondEngine") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& id, const Currency& ccy,
                                                        const string& creditCurveId, const bool hasCreditRisk,
                                                        const string& securityId, const string& referenceCurveId,
                                                        const string& incomeCurveId) override {

        string tsperiodStr = engineParameters_.at("TimestepPeriod");
        Period tsperiod = parsePeriod(tsperiodStr);
        Handle<YieldTermStructure> yts = market_->yieldCurve(referenceCurveId, configuration(MarketContext::pricing));

        Handle<YieldTermStructure> discountTS =
            market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        // fall back on reference curve if no income curve is specified
        Handle<YieldTermStructure> incomeTS = market_->yieldCurve(
            incomeCurveId.empty() ? referenceCurveId : incomeCurveId, configuration(MarketContext::pricing));
        Handle<DefaultProbabilityTermStructure> dpts;
        // credit curve may not always be used. If credit curve ID is empty proceed without it
        if (!creditCurveId.empty())
            dpts =
                securitySpecificCreditCurve(market_, securityId, creditCurveId, configuration(MarketContext::pricing))
                    ->curve();
        Handle<Quote> recovery;
        try {
            recovery = market_->recoveryRate(securityId, configuration(MarketContext::pricing));
        } catch (...) {
            // otherwise fall back on curve recovery
            WLOG("security specific recovery rate not found for security ID "
                 << securityId << ", falling back on the recovery rate for credit curve Id " << creditCurveId);
            if (!creditCurveId.empty())
                recovery = market_->recoveryRate(creditCurveId, configuration(MarketContext::pricing));
        }

        Handle<Quote> bondSpread;
        try {
            // spread is optional, pass empty handle to engine if not given (will be treated as 0 spread there)
            bondSpread = market_->securitySpread(securityId, configuration(MarketContext::pricing));
        } catch (...) {
        }

        if (!hasCreditRisk) {
            dpts = Handle<DefaultProbabilityTermStructure>();
        }

        return QuantLib::ext::make_shared<QuantExt::DiscountingForwardBondEngine>(discountTS, incomeTS, yts, bondSpread, dpts,
                                                                          recovery, tsperiod);
    }
};

} // namespace data
} // namespace ore

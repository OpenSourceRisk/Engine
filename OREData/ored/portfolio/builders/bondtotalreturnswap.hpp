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

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>
#include <qle/pricingengines/discountingbondtrsengine.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

class trsBondEngineBuilder : public CachingPricingEngineBuilder<string, const Currency&, const string&, const string&,
                                                                const string&, const string&> {
protected:
    trsBondEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"BondTRS"}) {}

    virtual string keyImpl(const Currency& ccy, const string& creditCurveId, const string& securityId,
                           const string& referenceCurveId, const string& adjustmentSpread) override {

        std::string returnString =
            ccy.code() + "_" + creditCurveId + "_" + securityId + "_" + referenceCurveId + "_" + adjustmentSpread;

        return returnString;
    }
};

class DiscountingBondTRSEngineBuilder : public trsBondEngineBuilder {
public:
    DiscountingBondTRSEngineBuilder()
        : trsBondEngineBuilder("DiscountedCashflows" /*the model; dont touch*/,
                               "DiscountingBondTRSEngine" /*the engine*/) {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const Currency& ccy, const string& creditCurveId,
                                                        const string& securityId, const string& referenceCurveId,
                                                        const string& adjustmentSpread) override {

        string tsperiodStr = engineParameters_.at("TimestepPeriod");
        Period tsperiod = parsePeriod(tsperiodStr);
        Handle<YieldTermStructure> bondReferenceYieldCurve =
            market_->yieldCurve(referenceCurveId, configuration(MarketContext::pricing));

        Handle<YieldTermStructure> oisCurve = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        Handle<DefaultProbabilityTermStructure> dpts;
        // credit curve may not always be used. If credit curve ID is empty proceed without it
        if (!creditCurveId.empty())
            dpts = market_->defaultCurve(creditCurveId, configuration(MarketContext::pricing));
        Handle<Quote> recovery;
        try {
            recovery = market_->recoveryRate(securityId, configuration(MarketContext::pricing));
        } catch (...) {
            // otherwise fall back on curve recovery
            ALOG("security specific recovery rate not found for security ID "
                 << securityId << ", falling back on the recovery rate for credit curve Id " << creditCurveId);
            if (!creditCurveId.empty())
                recovery = market_->recoveryRate(creditCurveId, configuration(MarketContext::pricing));
        }

        Handle<Quote> bondSpread = market_->securitySpread(securityId, configuration(MarketContext::pricing));

        LOG("Starting engine for bond trs: " << securityId << " with recovery rate: " << recovery->value()
                                             << " with spread adjustment: " << bondSpread->value());

        return boost::make_shared<QuantExt::DiscountingBondTRSEngine>(oisCurve, bondReferenceYieldCurve, bondSpread,
                                                                      dpts, recovery, tsperiod);
    }
};
} // namespace data
} // namespace ore

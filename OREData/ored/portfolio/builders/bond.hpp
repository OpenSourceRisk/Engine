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
\brief builder that returns an engine to price a bond instrument
\ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>

#include <qle/pricingengines/discountingriskybondengine.hpp>
#include <qle/pricingengines/discountingriskybondenginemultistate.hpp>

#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>

#include <boost/make_shared.hpp>

#include <regex>

namespace ore {
namespace data {

//! Engine Builder base class for Bonds
/*! Pricing engines are cached by security id
\ingroup builders
*/

class BondEngineBuilder : public CachingPricingEngineBuilder<string, const Currency&, const string&, const bool,
                                                             const string&, const string&> {
protected:
    BondEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"Bond"}) {}

    virtual string keyImpl(const Currency& ccy, const string& creditCurveId, const bool hasCreditRisk,
                           const string& securityId, const string& referenceCurveId) override {
        return ccy.code() + "_" + creditCurveId + "_" + securityId + "_" + referenceCurveId;
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
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& ccy, const string& creditCurveId,
                                                        const bool hasCreditRisk, const string& securityId,
                                                        const string& referenceCurveId) override {

        string tsperiodStr = engineParameter("TimestepPeriod");
        Period tsperiod = parsePeriod(tsperiodStr);
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
            WLOG("security specific recovery rate not found for security ID "
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

        return QuantLib::ext::make_shared<QuantExt::DiscountingRiskyBondEngine>(yts, dpts, recovery, spread, tsperiod);
    }
};

//! Multi State Engine Builder class for Bonds
/*! This creates a DiscountingRiskyBondEngineMultiState
    \ingroup portfolio
*/

class BondMultiStateDiscountingEngineBuilder : public BondEngineBuilder {
public:
    BondMultiStateDiscountingEngineBuilder()
        : BondEngineBuilder("DiscountedCashflows", "DiscountingRiskyBondEngineMultiState") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& ccy, const string& creditCurveId,
                                                        const bool hasCreditRisk, const string& securityId,
                                                        const string& referenceCurveId) override {
        string tsperiodStr = engineParameter("TimestepPeriod");
        Period tsperiod = parsePeriod(tsperiodStr);
        Handle<YieldTermStructure> yts = market_->yieldCurve(referenceCurveId, configuration(MarketContext::pricing));
        Handle<Quote> spread;
        try {
            spread = market_->securitySpread(securityId, configuration(MarketContext::pricing));
        } catch (...) {
        }
        // build state curves and recovery rates
        std::vector<Handle<DefaultProbabilityTermStructure>> dpts;
        std::vector<Handle<Quote>> recovery;
        Size mainResultState = Null<Size>();
        Size i = 0;
        for (; true; ++i) {
            std::ostringstream rule_s;
            rule_s << "Rule_" << i;
            if (engineParameters_.count(rule_s.str()) == 0)
                break;
            std::vector<std::string> tokens;
            std::string rule = engineParameters_[rule_s.str()];
            std::string stateCreditCurveId;
            // if rule is empty, we use the initial curve for this state
            // TODO computation should be optimised in this case, so that only different
            // curves are recomputed in the engine
            if (rule.empty()) {
                stateCreditCurveId = creditCurveId;
                DLOG("Rule " << rule_s.str() << " is empty, use initial curve " << stateCreditCurveId
                             << " for this state.");
            } else {
                boost::split(tokens, rule, boost::is_any_of(","));
                QL_REQUIRE(tokens.size() == 2, "invalid rule: " << rule);
                stateCreditCurveId = regex_replace(creditCurveId, std::regex(tokens[0]), tokens[1]);
                DLOG("Apply " << rule_s.str() << " => " << tokens[0] << " in " << creditCurveId << " yields state #"
                              << i << " creditCurve id " << stateCreditCurveId);
            }
            if (stateCreditCurveId == creditCurveId) {
                mainResultState = i;
                DLOG("State #" << i << " is the main result state (overwriting previous choice)");
            }
            Handle<DefaultProbabilityTermStructure> this_dpts;
            Handle<Quote> this_recovery;
            if (!stateCreditCurveId.empty() && hasCreditRisk) {
                this_dpts = market_->defaultCurve(stateCreditCurveId, configuration(MarketContext::pricing))->curve();
                this_recovery = market_->recoveryRate(stateCreditCurveId, configuration(MarketContext::pricing));
            }
            dpts.push_back(this_dpts);
            recovery.push_back(this_recovery);
        }
        // If there were no rules at all we take the original creditCurveId as the only state
        if (i == 0) {
            Handle<DefaultProbabilityTermStructure> this_dpts;
            Handle<Quote> this_recovery;
            if (!creditCurveId.empty() && hasCreditRisk) {
                this_dpts = market_->defaultCurve(creditCurveId, configuration(MarketContext::pricing))->curve();
                this_recovery = market_->recoveryRate(creditCurveId, configuration(MarketContext::pricing));
            }
            dpts.push_back(this_dpts);
            recovery.push_back(this_recovery);
            mainResultState = 0;
            DLOG("No rules given, only states are " << creditCurveId << " and default");
        }
        // check we have a main result state
        QL_REQUIRE(mainResultState != Null<Size>(),
                   "BondMultiStateEngineBuilder: No main state found for " << securityId << " / " << creditCurveId);
        // return engine
        return QuantLib::ext::make_shared<QuantExt::DiscountingRiskyBondEngineMultiState>(yts, dpts, recovery, mainResultState,
                                                                                  spread, tsperiod);
    }
};

} // namespace data
} // namespace ore

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
\brief Builder that returns an engine to price a credit default swap
\ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>

#include <qle/pricingengines/midpointcdsenginemultistate.hpp>

#include <ql/pricingengines/credit/midpointcdsengine.hpp>

#include <boost/make_shared.hpp>

#include <regex>

namespace ore {
namespace data {

/*! This class provides a key with which we will cache the CDS engine builders

    In general, the CDS engine builders will be cached by the credit curve Id of the reference entity and the currency
    of the trade that needs to be priced. If we are caching by credit curve Id and currency only, the recovery rate
    member should be \c Null<Real>().

    In some cases, for fixed recovery CDS trades for example, we need to cache the CDS engine builder not only by
    credit curve Id and currency but also with an exogenous recovery rate that we wish to use instead of the market
    supplied recovery rate.
*/
class CDSEngineKey {
public:
    /*! Constructor that takes a credit curve Id, \p creditCurveId, a currency, \p ccy, and optionally a recovery
        rate, \p recoveryRate.
    */
    CDSEngineKey(const std::string& creditCurveId, const QuantLib::Currency& ccy,
                 QuantLib::Real recoveryRate = QuantLib::Null<QuantLib::Real>())
        : creditCurveId_(creditCurveId), ccy_(ccy), recoveryRate_(recoveryRate) {}

    //! Return the credit curve Id
    const std::string& creditCurveId() const { return creditCurveId_; }

    //! Return the currency
    const QuantLib::Currency& currency() const { return ccy_; }

    //! Return the recovery rate if it is set, otherwise \c Null<Real>()
    QuantLib::Real recoveryRate() const { return recoveryRate_; }

private:
    std::string creditCurveId_;
    QuantLib::Currency ccy_;
    QuantLib::Real recoveryRate_;
};

inline bool operator==(const CDSEngineKey& lhs, const CDSEngineKey& rhs) {

    // Check the credit curve IDs first
    if (lhs.creditCurveId() != rhs.creditCurveId())
        return false;

    // Check the currencies
    if (lhs.currency() != rhs.currency())
        return false;

    // Now check recovery rates.
    if (lhs.recoveryRate() == QuantLib::Null<QuantLib::Real>() &&
        rhs.recoveryRate() == QuantLib::Null<QuantLib::Real>()) {
        // Most usual case is both are Null<Real> and using the recovery rate from the market.
        return true;
    } else if ((lhs.recoveryRate() != QuantLib::Null<QuantLib::Real>() &&
                rhs.recoveryRate() == QuantLib::Null<QuantLib::Real>()) ||
               (lhs.recoveryRate() == QuantLib::Null<QuantLib::Real>() &&
                rhs.recoveryRate() != QuantLib::Null<QuantLib::Real>())) {
        return false;
    } else {
        return QuantLib::close(lhs.recoveryRate(), rhs.recoveryRate());
    }
}

inline bool operator<(const CDSEngineKey& lhs, const CDSEngineKey& rhs) {

    // Check equality first
    if (lhs == rhs)
        return false;

    // Now check credit curve Ids.
    if (lhs.creditCurveId() != rhs.creditCurveId())
        return lhs.creditCurveId() < rhs.creditCurveId();

    // Now check currencies.
    if (lhs.currency() != rhs.currency())
        return lhs.currency().name() < rhs.currency().name();

    // Now use the recovery rates
    return lhs.recoveryRate() < rhs.recoveryRate();
}

inline bool operator!=(const CDSEngineKey& lhs, const CDSEngineKey& rhs) { return !(lhs == rhs); }
inline bool operator>(const CDSEngineKey& lhs, const CDSEngineKey& rhs) { return rhs < lhs; }
inline bool operator<=(const CDSEngineKey& lhs, const CDSEngineKey& rhs) { return !(lhs > rhs); }
inline bool operator>=(const CDSEngineKey& lhs, const CDSEngineKey& rhs) { return !(lhs < rhs); }

//! Engine builder base class for credit default swaps
/*! Pricing engines are cached by CDSEngineKey
    \ingroup builders
*/
class CreditDefaultSwapEngineBuilder
    : public CachingPricingEngineBuilder<CDSEngineKey, QuantLib::Currency, std::string, QuantLib::Real> {

protected:
    CreditDefaultSwapEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"CreditDefaultSwap"}) {}

    CDSEngineKey keyImpl(QuantLib::Currency ccy, std::string creditCurveId,
                         QuantLib::Real recoveryRate = QuantLib::Null<QuantLib::Real>()) override {
        return CDSEngineKey(creditCurveId, ccy, recoveryRate);
    }
};

//! Midpoint engine builder class for credit default swaps
/*! This class creates a MidPointCdsEngine
    \ingroup builders
*/
class MidPointCdsEngineBuilder : public CreditDefaultSwapEngineBuilder {
public:
    MidPointCdsEngineBuilder() : CreditDefaultSwapEngineBuilder("DiscountedCashflows", "MidPointCdsEngine") {}

protected:
    QuantLib::ext::shared_ptr<PricingEngine>
    engineImpl(QuantLib::Currency ccy, std::string creditCurveId,
               QuantLib::Real recoveryRate = QuantLib::Null<QuantLib::Real>()) override {

        auto cfg = configuration(MarketContext::pricing);
        auto yts = market_->discountCurve(ccy.code(), cfg);
        auto dpts = market_->defaultCurve(creditCurveId, cfg);

        if (recoveryRate == QuantLib::Null<QuantLib::Real>()) {
            // If recovery rate is null, get it from the market for the given reference entity
            recoveryRate = market_->recoveryRate(creditCurveId, cfg)->value();
        }

        return QuantLib::ext::make_shared<MidPointCdsEngine>(dpts->curve(), recoveryRate, yts);
    }
};

//! Multi State Engine Builder class for CDS
/*! This creates a CreditDefaultSwapEngineMultiState
    \ingroup portfolio
*/

class MidPointCdsMultiStateEngineBuilder : public CreditDefaultSwapEngineBuilder {
public:
    MidPointCdsMultiStateEngineBuilder()
        : CreditDefaultSwapEngineBuilder("DiscountedCashflows", "MidPointCdsEngineMultiState") {}

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine>
    engineImpl(QuantLib::Currency ccy, std::string creditCurveId,
               QuantLib::Real recoveryRate = QuantLib::Null<QuantLib::Real>()) override {
        Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
        // build state curves and recovery rates
        // big overlap with BondMultiStateEngineBuilder
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
            dpts.push_back(market_->defaultCurve(stateCreditCurveId, configuration(MarketContext::pricing))->curve());
            recovery.push_back(recoveryRate == Null<Real>()
                                   ? market_->recoveryRate(stateCreditCurveId, configuration(MarketContext::pricing))
                                   : Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(recoveryRate)));
        }
        // If there were no rules at all we take the original creditCurveId as the only state
        if (i == 0) {
            dpts.push_back(market_->defaultCurve(creditCurveId, configuration(MarketContext::pricing))->curve());
            recovery.push_back(recoveryRate == Null<Real>()
                                   ? market_->recoveryRate(creditCurveId, configuration(MarketContext::pricing))
                                   : Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(recoveryRate)));
            mainResultState = 0;
            DLOG("No rules given, only states are " << creditCurveId << " and default");
        }
        // check we have a main result state
        QL_REQUIRE(mainResultState != Null<Size>(),
                   "CreditDefaultSwapMultiStateEngineBuilder: No main state found for " << creditCurveId);
        // return engine
        return QuantLib::ext::make_shared<QuantExt::MidPointCdsEngineMultiState>(dpts, recovery, yts, mainResultState);
    }
};

} // namespace data
} // namespace ore

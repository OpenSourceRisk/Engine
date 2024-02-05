/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/blackmultilegoptionengine.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/instruments/rebatedexercise.hpp>

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>

#include <boost/algorithm/string/join.hpp>

namespace QuantExt {

BlackMultiLegOptionEngineBase::BlackMultiLegOptionEngineBase(const Handle<YieldTermStructure>& discountCurve,
                                                             const Handle<SwaptionVolatilityStructure>& volatility)
    : discountCurve_(discountCurve), volatility_(volatility) {}

bool BlackMultiLegOptionEngineBase::instrumentIsHandled(const MultiLegOption& m, std::vector<std::string>& messages) {
    return instrumentIsHandled(m.legs(), m.payer(), m.currency(), m.exercise(), m.settlementType(),
                               m.settlementMethod(), messages);
}

bool BlackMultiLegOptionEngineBase::instrumentIsHandled(const std::vector<Leg>& legs, const std::vector<bool>& payer,
                                                        const std::vector<Currency>& currency,
                                                        const boost::shared_ptr<Exercise>& exercise,
                                                        const Settlement::Type& settlementType,
                                                        const Settlement::Method& settlementMethod,
                                                        std::vector<std::string>& messages) {

    bool isHandled = true;

    // is there a unique pay currency and all interest rate indices are in this same currency?

    for (Size i = 1; i < currency.size(); ++i) {
        if (currency[0] != currency[i]) {
            messages.push_back("NumericLgmMultilegOptionEngine: can only handle single currency underlyings, got " +
                               currency[0].code() + " on leg #1 and " + currency[i].code() + " on leg #" +
                               std::to_string(i + 1));
            isHandled = false;
        }
    }

    for (Size i = 0; i < legs.size(); ++i) {
        for (Size j = 0; j < legs[i].size(); ++j) {
            if (auto cpn = boost::dynamic_pointer_cast<FloatingRateCoupon>(legs[i][j])) {
                if (cpn->index()->currency() != currency[0]) {
                    messages.push_back("NumericLgmMultilegOptionEngine: can only handle indices (" +
                                       cpn->index()->name() + ") with same currency as unqiue pay currency (" +
                                       currency[0].code());
                }
            }
        }
    }

    // check coupon types

    for (Size i = 0; i < legs.size(); ++i) {
        for (Size j = 0; j < legs[i].size(); ++j) {
            if (auto c = boost::dynamic_pointer_cast<Coupon>(legs[i][j])) {
                if (!(boost::dynamic_pointer_cast<IborCoupon>(c) || boost::dynamic_pointer_cast<FixedRateCoupon>(c) ||
                      boost::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(c) ||
                      boost::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(c) ||
                      boost::dynamic_pointer_cast<QuantLib::AverageBMACoupon>(c) ||
                      boost::dynamic_pointer_cast<QuantExt::SubPeriodsCoupon1>(c))) {
                    messages.push_back(
                        "BlackMultilegOptionEngine: coupon type not handled, supported coupon types: Fix, "
                        "Ibor, ON comp, ON avg, BMA/SIFMA, subperiod. leg = " +
                        std::to_string(i) + " cf = " + std::to_string(j));
                    isHandled = false;
                }
            }
        }
    }

    // check exercise type

    isHandled = isHandled && exercise->type() == Exercise::European;

    return isHandled;
}

void BlackMultiLegOptionEngineBase::calculate() const {

    std::vector<std::string> messages;
    QL_REQUIRE(instrumentIsHandled(legs_, payer_, currency_, exercise_, settlementType_, settlementMethod_, messages),
               "BlackMultiLegOptionEngineBase::calculate(): instrument is not handled: "
                   << boost::algorithm::join(messages, ", "));
}

BlackMultiLegOptionEngine::BlackMultiLegOptionEngine(const Handle<YieldTermStructure>& discountCurve,
                                                     const Handle<SwaptionVolatilityStructure>& volatility)
    : BlackMultiLegOptionEngineBase(discountCurve, volatility) {
    registerWith(discountCurve_);
    registerWith(volatility_);
}

void BlackMultiLegOptionEngine::calculate() const {
    legs_ = arguments_.legs;
    payer_ = arguments_.payer;
    currency_ = arguments_.currency;
    exercise_ = arguments_.exercise;
    settlementType_ = arguments_.settlementType;
    settlementMethod_ = arguments_.settlementMethod;

    BlackMultiLegOptionEngineBase::calculate();

    results_.value = npv_;
    results_.underlyingNpv = underlyingNpv_;
    results_.additionalResults = additionalResults_;
    results_.additionalResults["underlyingNpv"] = underlyingNpv_;
}

BlackSwaptionFromMultilegOptionEngine::BlackSwaptionFromMultilegOptionEngine(
    const Handle<YieldTermStructure>& discountCurve, const Handle<SwaptionVolatilityStructure>& volatility)
    : BlackMultiLegOptionEngineBase(discountCurve, volatility) {
    registerWith(discountCurve_);
    registerWith(volatility_);
}

void BlackSwaptionFromMultilegOptionEngine::calculate() const {
    legs_ = arguments_.legs;
    payer_.resize(arguments_.payer.size());
    for (Size i = 0; i < arguments_.payer.size(); ++i) {
        payer_[i] = QuantLib::close_enough(arguments_.payer[i], -1.0);
    }
    currency_ = std::vector<Currency>(legs_.size(), arguments_.swap->iborIndex()->currency());
    exercise_ = arguments_.exercise;
    settlementType_ = arguments_.settlementType;
    settlementMethod_ = arguments_.settlementMethod;

    BlackMultiLegOptionEngineBase::calculate();

    results_.value = npv_;
    results_.additionalResults = additionalResults_;
    results_.additionalResults["underlyingNpv"] = underlyingNpv_;
}

BlackNonstandardSwaptionFromMultilegOptionEngine::BlackNonstandardSwaptionFromMultilegOptionEngine(
    const Handle<YieldTermStructure>& discountCurve, const Handle<SwaptionVolatilityStructure>& volatility)
    : BlackMultiLegOptionEngineBase(discountCurve, volatility) {
    registerWith(discountCurve_);
    registerWith(volatility_);
}

void BlackNonstandardSwaptionFromMultilegOptionEngine::calculate() const {
    legs_ = arguments_.legs;
    for (Size i = 0; i < arguments_.payer.size(); ++i) {
        payer_[i] = QuantLib::close_enough(arguments_.payer[i], -1.0);
    }
    currency_ = std::vector<Currency>(legs_.size(), arguments_.swap->iborIndex()->currency());
    exercise_ = arguments_.exercise;
    settlementType_ = arguments_.settlementType;
    settlementMethod_ = arguments_.settlementMethod;

    BlackMultiLegOptionEngineBase::calculate();

    results_.value = npv_;
    results_.additionalResults = additionalResults_;
    results_.additionalResults["underlyingNpv"] = underlyingNpv_;
}

} // namespace QuantExt

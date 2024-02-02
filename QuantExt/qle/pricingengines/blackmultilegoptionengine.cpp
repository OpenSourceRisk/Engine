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

namespace QuantExt {

BlackMultiLegOptionEngineBase::BlackMultiLegOptionEngineBase(const Handle<YieldTermStructure>& discountCurve,
                                                             const Handle<SwaptionVolatilityStructure>& volatility)
    : discountCurve_(discountCurve), volatility_(volatility) {}

void BlackMultiLegOptionEngineBase::calculate() const {
    // TODO...
}

BlackMultiLegOptionEngine(const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
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

    NumericLgmMultiLegOptionEngineBase::calculate();

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
    payer_ = arguments_.payer;
    currency_ = std::vector<Currency>(legs_.size(), arguments_.swap->iborIndex()->currency());
    exercise_ = arguments_.exercise;
    settlementType_ = arguments_.settlementType;
    settlementMethod_ = arguments_.settlementMethod;

    NumericLgmMultiLegOptionEngineBase::calculate();

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
    payer_ = arguments_.payer;
    currency_ = std::vector<Currency>(legs_.size(), arguments_.swap->iborIndex()->currency());
    exercise_ = arguments_.exercise;
    settlementType_ = arguments_.settlementType;
    settlementMethod_ = arguments_.settlementMethod;

    NumericLgmMultiLegOptionEngineBase::calculate();

    results_.value = npv_;
    results_.additionalResults = additionalResults_;
    results_.additionalResults["underlyingNpv"] = underlyingNpv_;
}

} // namespace QuantExt

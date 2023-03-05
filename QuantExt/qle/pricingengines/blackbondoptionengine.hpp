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

/*! \file qle/pricingengines/blackbondoptionengine.hpp
\brief Black bond option engine
\ingroup engines
*/

#pragma once

#include <qle/instruments/bondoption.hpp>

#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Black-formula bond option engine
class BlackBondOptionEngine : public BondOption::engine {
public:
    //! volatility is the quoted fwd yield volatility, not price vol
    BlackBondOptionEngine(
        const Handle<YieldTermStructure>& discountCurve, const Handle<SwaptionVolatilityStructure>& volatility,
        const Handle<YieldTermStructure>& underlyingReferenceCurve,
        const Handle<DefaultProbabilityTermStructure>& defaultCurve = Handle<DefaultProbabilityTermStructure>(),
        const Handle<Quote>& recoveryRate = Handle<Quote>(), const Handle<Quote>& securitySpread = Handle<Quote>(),
        Period timestepPeriod = 1 * Months);
    void calculate() const override;

private:
    Handle<YieldTermStructure> discountCurve_;
    Handle<SwaptionVolatilityStructure> volatility_;
    Handle<YieldTermStructure> underlyingReferenceCurve_;
    Handle<DefaultProbabilityTermStructure> defaultCurve_;
    Handle<Quote> recoveryRate_;
    Handle<Quote> securitySpread_;
    Period timestepPeriod_;
};
} // namespace QuantExt

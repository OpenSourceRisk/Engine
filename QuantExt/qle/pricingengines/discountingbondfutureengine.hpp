/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/discountingbondfutureengine.hpp
    \brief Engine to value a Bond Future

    \ingroup engines
*/

#pragma once

#include <qle/instruments/bondfuture.hpp>

#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

//! Discounting Bond Future Engine
class DiscountingBondFutureEngine : public QuantExt::BondFuture::engine {
public:
    DiscountingBondFutureEngine(const Handle<YieldTermStructure>& discountCurve, const Handle<Quote>& conversionFactor);

    void calculate() const override;

    const Handle<YieldTermStructure>& discountCurve() const { return discountCurve_; }
    const Handle<Quote>& conversionFactor() const { return conversionFactor_; }

private:
    Handle<YieldTermStructure> discountCurve_;
    Handle<Quote> conversionFactor_;
};

} // namespace QuantExt

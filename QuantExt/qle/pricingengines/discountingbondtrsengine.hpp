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

/*! \file qle/pricingengines/discountingbondtrsengine.hpp
    \brief Engine to value a Bond TRS

    \ingroup engines
*/

#pragma once

#include <qle/instruments/bondtotalreturnswap.hpp>

#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/period.hpp>

namespace QuantExt {

//! Discounting Bond TRS Engine

/*!
  \ingroup engines
*/
class DiscountingBondTRSEngine : public QuantExt::BondTRS::engine {
public:
    DiscountingBondTRSEngine(const Handle<YieldTermStructure>& discountCurve);

    void calculate() const override;

    const Handle<YieldTermStructure>& discountCurve() const { return discountCurve_; }

private:
    Real calculateBondNpv(const Date& npvDate, const Date& start, const Date& end,
                          const Handle<YieldTermStructure>& discountCurve) const;

    const Handle<YieldTermStructure> discountCurve_;
};
} // namespace QuantExt

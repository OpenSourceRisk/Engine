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

/*! \file qle/termstructures/constantyoyinflationoptionletvolatility.hpp
    \brief constant yoy inflation volatility based on quote
 */

#pragma once

#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>

#include <ql/quote.hpp>

namespace QuantExt {

using namespace QuantLib;

//! Constant surface, no K or T dependence.
class ConstantYoYOptionletVolatility : public QuantLib::YoYOptionletVolatilitySurface {
public:
    //! \name Constructor
    //@{
    //! calculate the reference date based on the global evaluation date
    ConstantYoYOptionletVolatility(const Handle<Quote>& volatility, Natural settlementDays, const Calendar&,
                                   BusinessDayConvention bdc, const DayCounter& dc, const Period& observationLag,
                                   Frequency frequency, bool indexIsInterpolated);
    //@}
    virtual ~ConstantYoYOptionletVolatility() {}

    //! \name Limits
    //@{
    virtual Date maxDate() const { return Date::maxDate(); }
    //! the minimum strike for which the term structure can return vols
    virtual Real minStrike() const { return -QL_MAX_REAL; }
    //! the maximum strike for which the term structure can return vols
    virtual Real maxStrike() const { return QL_MAX_REAL; }
    //@}

protected:
    //! implements the actual volatility calculation in derived classes
    virtual Volatility volatilityImpl(Time length, Rate strike) const;

    Handle<Quote> volatility_;
};

} // namespace QuantExt

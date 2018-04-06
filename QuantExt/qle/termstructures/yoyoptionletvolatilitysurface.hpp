/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/yoyoptionletvolatilitysurface.hpp
    \brief YoY Inflation volatility surface - extends QuantLib YoYOptionletVolatilitySurface
           to include a volatility type and displacement
    \ingroup termstructures
*/

#ifndef quantext_yoy_optionlet_volatility_surface_hpp
#define quantext_yoy_optionlet_volatility_surface_hpp

#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>
#include <ql/termstructures/volatility/volatilitytype.hpp>

using namespace QuantLib;

namespace QuantExt {

//! YoY Inflation volatility surface
/*! \ingroup termstructures */
class YoYOptionletVolatilitySurface : public QuantLib::YoYOptionletVolatilitySurface {
public:

    //! \name Constructor
    //! calculate the reference date based on the global evaluation date
    YoYOptionletVolatilitySurface(Natural settlementDays,
        const Calendar& cal, BusinessDayConvention bdc,
        const DayCounter& dc, const Period& observationLag,
        Frequency frequency, bool indexIsInterpolated) : 
        QuantLib::YoYOptionletVolatilitySurface(settlementDays, cal, bdc, dc, observationLag, frequency, indexIsInterpolated) {}

    virtual QuantLib::VolatilityType volatilityType() const;
    virtual Real displacement() const;
};

inline QuantLib::VolatilityType YoYOptionletVolatilitySurface::volatilityType() const {
    return ShiftedLognormal;
}

inline Real YoYOptionletVolatilitySurface::displacement() const {
    return 0.0;
}

} // namespace QuantExt

#endif
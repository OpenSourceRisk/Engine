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

/*! \file strikeinterpolatedyoyoptionletvolatilitysurface.hpp
    \brief Interpolated yoy optionlet volatility, derived from QuantLib's to control extrapolation
*/

#ifndef quantext_strike_interpolated_yoy_optionlet_volatility_surface_hpp
#define quantext_strike_interpolated_yoy_optionlet_volatility_surface_hpp

#include <ql/experimental/inflation/kinterpolatedyoyoptionletvolatilitysurface.hpp>

namespace QuantExt {

//! Strike-interpolated YoY optionlet volatility
/*! The stripper provides curves in the T direction along each K.
    We don't know whether this is interpolating or fitting in the
    T direction.  Our K direction interpolations are not model
    fitting.

    An alternative design would be a
    FittedYoYOptionletVolatilitySurface taking a model, e.g. SABR
    in the interest rate world.  This could use the same stripping
    in the T direction along each K.

    \bug Tests currently fail.
*/
template <class Interpolator1D>
class StrikeInterpolatedYoYOptionletVolatilitySurface
    : public QuantLib::KInterpolatedYoYOptionletVolatilitySurface<Interpolator1D> {
public:
    //! \name Constructor
    //! calculate the reference date based on the global evaluation date
    StrikeInterpolatedYoYOptionletVolatilitySurface(const Natural settlementDays, const Calendar&,
                                                    const BusinessDayConvention bdc, const DayCounter& dc,
                                                    const Period& lag,
                                                    const ext::shared_ptr<YoYCapFloorTermPriceSurface>& capFloorPrices,
                                                    const ext::shared_ptr<YoYInflationCapFloorEngine>& pricer,
                                                    const ext::shared_ptr<YoYOptionletStripper>& yoyOptionletStripper,
                                                    const Real slope,
                                                    const Interpolator1D& interpolator = Interpolator1D());
    virtual Real minStrike() const;
    virtual Real maxStrike() const;

protected:
    virtual Volatility volatilityImpl(const Date& d, Rate strike) const;
};

// template definitions

template <class Interpolator1D>
StrikeInterpolatedYoYOptionletVolatilitySurface<Interpolator1D>::StrikeInterpolatedYoYOptionletVolatilitySurface(
    const Natural settlementDays, const Calendar& cal, const BusinessDayConvention bdc, const DayCounter& dc,
    const Period& lag, const ext::shared_ptr<YoYCapFloorTermPriceSurface>& capFloorPrices,
    const ext::shared_ptr<YoYInflationCapFloorEngine>& pricer,
    const ext::shared_ptr<YoYOptionletStripper>& yoyOptionletStripper, const Real slope,
    const Interpolator1D& interpolator)
    : QuantLib::KInterpolatedYoYOptionletVolatilitySurface<Interpolator1D>(
          settlementDays, cal, bdc, dc, lag, capFloorPrices, pricer, yoyOptionletStripper, slope, interpolator) {}

template <class Interpolator1D>
Real StrikeInterpolatedYoYOptionletVolatilitySurface<Interpolator1D>::minStrike() const {
    if (this->allowsExtrapolation())
        return QL_MIN_REAL;
    else
        return this->capFloorPrices_->strikes().front();
}

template <class Interpolator1D>
Real StrikeInterpolatedYoYOptionletVolatilitySurface<Interpolator1D>::maxStrike() const {
    if (this->allowsExtrapolation())
        return QL_MAX_REAL;
    else
        return this->capFloorPrices_->strikes().back();
}

template <class Interpolator1D>
Volatility StrikeInterpolatedYoYOptionletVolatilitySurface<Interpolator1D>::volatilityImpl(const Date& d,
                                                                                           Rate strike) const {
    Real kmax = this->capFloorPrices_->strikes().back();
    Real kmin = this->capFloorPrices_->strikes().front();
    Real tmpStrike = (kmax + kmin) / 2;

    // call the base class' function such that it does not throw, i.e. with a valid strike, this resets
    // tempKinterpolation_
    KInterpolatedYoYOptionletVolatilitySurface<Interpolator1D>::volatilityImpl(d, tmpStrike);

    // now enable extrapolation...
    if (this->allowsExtrapolation())
        this->tempKinterpolation_.enableExtrapolation();

    // ...and return the vol for the strike we want
    return this->tempKinterpolation_(strike);
}

} // namespace QuantExt

#endif

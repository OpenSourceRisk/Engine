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

/*! \file qle/termstructures/equityvolconstantspread.hpp
    \brief equity surface that combines an ATM curve and vol spreads from a surface
    \ingroup termstructures
*/

#ifndef quantext_equityvolatilityconstantspread_hpp
#define quantext_equityvolatilityconstantspread_hpp

#include <boost/shared_ptr.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Equity cube that combines an ATM matrix and vol spreads from a cube
/*! Notice that the TS has a floating reference date and accesses the source TS only via
 their time-based volatility methods.

 \warning the given atm vol structure should be strike independent, this is not checked
*/
class EquityVolatilityConstantSpread : public BlackVolTermStructure {
public:
    EquityVolatilityConstantSpread(const Handle<BlackVolTermStructure>& atm,
                                   const Handle<BlackVolTermStructure>& surface)
        : BlackVolTermStructure(0, atm->calendar(), atm->businessDayConvention(), atm->dayCounter()), atm_(atm),
          surface_(surface) {
        enableExtrapolation(atm->allowsExtrapolation());
        registerWith(atm);
        registerWith(surface);
    }

    //! \name TermStructure interface
    //@{
    DayCounter dayCounter() const { return atm_->dayCounter(); }
    Date maxDate() const { return atm_->maxDate(); }
    Time maxTime() const { return atm_->maxTime(); }
    const Date& referenceDate() const { return atm_->referenceDate(); }
    Calendar calendar() const { return atm_->calendar(); }
    Natural settlementDays() const { return atm_->settlementDays(); }
    //@}
    //! \name VolatilityTermStructure interface
    //@{
    Rate minStrike() const { return surface_->minStrike(); }
    Rate maxStrike() const { return surface_->maxStrike(); }
    //@}

protected:
    Volatility blackVolImpl(Time t, Rate strike) const {
        Real s = surface_->blackVol(t, strike, true) - surface_->blackVol(t, Null<Real>(), true);
        Real v = atm_->blackVol(t, Null<Real>(), true);
        return v + s;
    }

    Real blackVarianceImpl(Time t, Real strike) const {
        Real vol = blackVolImpl(t, strike);
        return vol * vol * t;
    }

private:
    Handle<BlackVolTermStructure> atm_, surface_;
};

} // namespace QuantExt

#endif

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

/*! \file qle/termstructures/blackvolsurfacewithatm.hpp
    \brief Wrapper class for a BlackVolTermStructure that easily exposes ATM vols.
    \ingroup termstructures
*/

#ifndef quantext_blackvolsurfacewithatm_hpp
#define quantext_blackvolsurfacewithatm_hpp

#include <boost/shared_ptr.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Wrapper class for a BlackVolTermStructure that easily exposes ATM vols.
/*! This class implements BlackVolatilityTermStructure and takes a surface (well, any BlackVolTermStructure) as an
    input. If asked for a volatility with strike=Null<Real>() or 0 it will calculate the forward value and use this as
    the strike, this makes it easy to access ATMF values.

    The forward value is calculated using the input spot and yield curves, so can be used for both FX and Equity vols.

    For FX markets, one should set the spot to be the FX spot rate, yield1 to be the base discount curve and yield2 to
    be the reference discount curve (e.g. EURUSD, yield1 = EUR).

    For Equity markets, one should set the spot to be the equity price, yield1 to be the discount curve and yield2 to
    be the dividend curve.
 */
//!\ingroup termstructures

class BlackVolatilityWithATM : public BlackVolatilityTermStructure {
public:
    //! Constructor. This is a floating term structure (settlement days is zero)
    BlackVolatilityWithATM(const boost::shared_ptr<BlackVolTermStructure>& surface, const Handle<Quote>& spot,
                           const Handle<YieldTermStructure>& yield1, const Handle<YieldTermStructure>& yield2);

    //! \name TermStructure interface
    //@{
    DayCounter dayCounter() const { return surface_->dayCounter(); }
    Date maxDate() const { return surface_->maxDate(); }
    Time maxTime() const { return surface_->maxTime(); }
    const Date& referenceDate() const { return surface_->referenceDate(); }
    Calendar calendar() const { return surface_->calendar(); }
    Natural settlementDays() const { return surface_->settlementDays(); }
    //! \name VolatilityTermStructure interface
    //@{
    Rate minStrike() const { return surface_->minStrike(); }
    Rate maxStrike() const { return surface_->maxStrike(); }
    //@}

protected:
    // Here we check if strike is Null<Real>() or 0 and calculate ATMF if so.
    Volatility blackVolImpl(Time t, Real strike) const;

private:
    boost::shared_ptr<BlackVolTermStructure> surface_;
    Handle<Quote> spot_;
    Handle<YieldTermStructure> yield1_, yield2_;
};

} // namespace QuantExt

#endif

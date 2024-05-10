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

#include <ql/shared_ptr.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

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
    BlackVolatilityWithATM(const QuantLib::ext::shared_ptr<BlackVolTermStructure>& surface, const Handle<Quote>& spot,
                           const Handle<YieldTermStructure>& yield1 = Handle<YieldTermStructure>(),
                           const Handle<YieldTermStructure>& yield2 = Handle<YieldTermStructure>());

    //! \name TermStructure interface
    //@{
    DayCounter dayCounter() const override { return surface_->dayCounter(); }
    Date maxDate() const override { return surface_->maxDate(); }
    Time maxTime() const override { return surface_->maxTime(); }
    const Date& referenceDate() const override { return surface_->referenceDate(); }
    Calendar calendar() const override { return surface_->calendar(); }
    Natural settlementDays() const override { return surface_->settlementDays(); }
    //@}

    //! \name VolatilityTermStructure interface
    //@{
    Rate minStrike() const override { return surface_->minStrike(); }
    Rate maxStrike() const override { return surface_->maxStrike(); }
    //@}

    //! \name Inspectors
    //@{
    QuantLib::ext::shared_ptr<BlackVolTermStructure> surface() const { return surface_; }
    Handle<Quote> spot() const { return spot_; }
    Handle<YieldTermStructure> yield1() const { return yield1_; }
    Handle<YieldTermStructure> yield2() const { return yield2_; }
    //@}

protected:
    // Here we check if strike is Null<Real>() or 0 and calculate ATMF if so.
    Volatility blackVolImpl(Time t, Real strike) const override;

private:
    QuantLib::ext::shared_ptr<BlackVolTermStructure> surface_;
    Handle<Quote> spot_;
    Handle<YieldTermStructure> yield1_, yield2_;
};

} // namespace QuantExt

#endif

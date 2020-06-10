/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

namespace QuantExt {
using namespace QuantLib;

//! Wrapper class for a BlackVolTermStructure that allows us to proxy one surface off another.
/*! This class implements BlackVolatilityTermStructure and takes a surface (well, any BlackVolTermStructure) as an
    input. It also that's Handles to two Quotes (spot and proxySpot), where spot is the current quote of the underlying 
    for the surface being constructed and proxySpot is the current quote for the surface being proxied off.

    The vol returned from the new surface is proxied from the base, adjusting to match ATM:

    \f{eqnarray}{
    \sigma_2(K,T) = \sigma_1(\frac{K}{S_2}*S_1,T)
    \f}

    */
    //!\ingroup termstructures

class BlackVolatilitySurfaceProxy : public BlackVolatilityTermStructure {
public:
    //! Constructor. This is a floating term structure (settlement days is zero)
    BlackVolatilitySurfaceProxy(const boost::shared_ptr<BlackVolTermStructure>& proxySurface, 
        const Handle<Quote>& spot, const Handle<Quote>& proxySpot);

    //! \name TermStructure interface
    //@{
    DayCounter dayCounter() const { return proxySurface_->dayCounter(); }
    Date maxDate() const { return proxySurface_->maxDate(); }
    Time maxTime() const { return proxySurface_->maxTime(); }
    const Date& referenceDate() const { return proxySurface_->referenceDate(); }
    Calendar calendar() const { return proxySurface_->calendar(); }
    Natural settlementDays() const { return proxySurface_->settlementDays(); }
    //@}

    //! \name VolatilityTermStructure interface
    //@{
    Rate minStrike() const { return proxySurface_->minStrike(); }
    Rate maxStrike() const { return proxySurface_->maxStrike(); }
    //@}

    //! \name Inspectors
    //@{
    boost::shared_ptr<BlackVolTermStructure> proxySurface() const { return proxySurface_; }
    Handle<Quote> spot() const { return spot_; }
    Handle<Quote> proxySpot() const { return proxySpot_; }
    //@}

protected:
    // Here we adjust the returned vol.
    Volatility blackVolImpl(Time t, Real strike) const;

private:
    boost::shared_ptr<BlackVolTermStructure> proxySurface_;
    Handle<Quote> spot_, proxySpot_;
};

} // namespace QuantExt

#endif

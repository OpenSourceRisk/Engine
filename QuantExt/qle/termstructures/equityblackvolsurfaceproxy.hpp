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
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/indexes/equityindex.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Wrapper class for a BlackVolTermStructure that allows us to proxy one equity vol surface off another.
/*! This class implements BlackVolatilityTermStructure and takes a surface (well, any BlackVolTermStructure) as an
    input. It also takes Handles to two EquityIndices (index and proxyIndex), where index is the 'EquityIndex' of the
   underlying for the surface being constructed and proxyIndex is the 'EquityIndex' for the surface being proxied off.

    The vol returned from the new surface is proxied from the base, adjusting by the forward prices to match ATM:

    \f{eqnarray}{
    \sigma_2(K,T) = \sigma_1(\frac{K}{F_2}*F_1,T)
    \f}

    Where \n\n
    \f$ \sigma_1 = \text{Volatility of underlying being proxied against}\f$ \n
    \f$ \sigma_2 = \text{Volatility of underlying being proxied}\f$ \n
    \f$ F_1 = \text{Forward at time T of the underlying being proxied against}\f$ \n
    \f$ F_2 = \text{Forward at time T of the underlying being proxied}\f$ \n
    \f$ T = \text{Time}\f$
    \n

    Note: This surface only proxies equity volatilities, this is because we are forced to look up the equity fixings
    using time instead of date and use the forecastFixing method in an EquityIndex. A more general class could be
   developed if need, using Index instead of EquityIndex, if the time lookup could be overcome.

    */
//!\ingroup termstructures

class EquityBlackVolatilitySurfaceProxy : public BlackVolatilityTermStructure {
public:
    //! Constructor. This is a floating term structure (settlement days is zero)
    EquityBlackVolatilitySurfaceProxy(const boost::shared_ptr<BlackVolTermStructure>& proxySurface,
                                      const boost::shared_ptr<EquityIndex>& index,
                                      const boost::shared_ptr<EquityIndex>& proxyIndex);

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
    Rate minStrike() const;
    Rate maxStrike() const;
    //@}

    //! \name Inspectors
    //@{
    boost::shared_ptr<BlackVolTermStructure> proxySurface() const { return proxySurface_; }
    boost::shared_ptr<EquityIndex> index() const { return index_; }
    boost::shared_ptr<EquityIndex> proxyIndex() const { return proxyIndex_; }
    //@}

protected:
    // Here we adjust the returned vol.
    Volatility blackVolImpl(Time t, Real strike) const;

private:
    boost::shared_ptr<BlackVolTermStructure> proxySurface_;
    boost::shared_ptr<EquityIndex> index_, proxyIndex_;
};

} // namespace QuantExt

#endif

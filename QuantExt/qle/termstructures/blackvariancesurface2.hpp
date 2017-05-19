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

/*! \file blackvariancesurface2.hpp
    \brief Black volatility surface for equity markets
*/

#ifndef quantext_black_variance_surface_hpp
#define quantext_black_variance_surface_hpp

#include <ql/math/interpolation.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Black volatility surface modeled as variance surface
/*! This class calculates time/strike dependent Black volatilities using as input Black volatilities observed
    in the market.

    Unlike QuantLib::BlackVarianceSurface, this class does not assume that the same strikes are used for each
    expiry, as is typically the case in Equity markets. Instead it takes a separate set of strikes for each expiry.

    Interpolation is a bit more involved as we do not have a simple 2-D grid.
    Firstly a smile is built for each fixed expiry date by interpolation of volatility, the default interpolation
    here is Cubic and this can be overridden with setInterpolation below.  Then, for any time T and strike K, we
    calculate the variance at K for each expiry date and this is feed into a second interpolator (hardcoded to Linear)
    which returns the required volatility. This works fine if the time T is close to one of the input expiries or
    if there is not a significant forward drift (i.e. if the discount and dividend yields are comparable), in other
    cases it would be preferred to interpolate in forward, rather than spot, space.

    Extrapolation is enabled by default on both sets of interpolators, and hence maxDate, minStrike and maxStrike
    are all set to numeric limits.

    \note This is potentially slow with a large number of expires as we are calculating volatilities 
          for each expiry even though we are only using 1 or 2 of them (due to linear interpolation).

    \todo Make the second interpolator configurable (requires templating)

    \todo Rather than calculate the variance at each expiry for K, it would be better to move the strike K
          up and down the forward curve for each expiry and so do linear interpolation of variance in
          the forward space. This would require the class to take discount and dividend curves as
          inputs.
*/
class BlackVarianceSurface2 : public BlackVarianceTermStructure {
public:
    BlackVarianceSurface2(const Date& referenceDate, const Calendar& cal, const std::vector<Date>& dates,
                          const std::vector<std::vector<Real> >& strikes,
                          const std::vector<std::vector<Volatility> >& vols, const DayCounter& dayCounter);
    //! \name TermStructure interface
    //@{
    DayCounter dayCounter() const { return dayCounter_; }
    Date maxDate() const { return Date::maxDate(); }
    //@}

    //! \name VolatilityTermStructure interface
    //@{
    Real minStrike() const { return 0.0; }
    Real maxStrike() const { return QL_MAX_REAL; }
    //@}

    //! \name Modifiers
    //@{
    template <class Interpolator> void setInterpolation(const Interpolator& i = Interpolator()) {

        // Build one interpolator for each maturity
        interpolators_.resize(vols_.size());
        for (QuantLib::Size j = 0; j < vols_.size(); j++) {
            interpolators_[j] = i.interpolate(strikes_[j].begin(), strikes_[j].end(), vols_[j].begin());
            interpolators_[j].enableExtrapolation();
            interpolators_[j].update();
        }
        notifyObservers();
    }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&);
    //@}

protected:
    virtual Real blackVarianceImpl(Time t, Real strike) const;

private:
    DayCounter dayCounter_;
    std::vector<Time> times_;
    std::vector<std::vector<Real> > strikes_;
    std::vector<std::vector<Volatility> > vols_;
    std::vector<Interpolation> interpolators_;
};

// inline definitions
inline void BlackVarianceSurface2::accept(AcyclicVisitor& v) {
    Visitor<BlackVarianceSurface2>* v1 = dynamic_cast<Visitor<BlackVarianceSurface2>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        BlackVarianceTermStructure::accept(v);
}

} // namespace QuantExt

#endif

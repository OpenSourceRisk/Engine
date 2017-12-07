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

/*! \file blackvariancesurfacemoneyness.hpp
 \brief Black volatility surface based on forward moneyness
 \ingroup termstructures
 */

#ifndef quantext_black_variance_surface_moneyness_hpp
#define quantext_black_variance_surface_moneyness_hpp

#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Abstract Black volatility surface based on moneyness (moneyness defined in subclasses)
//!  \ingroup termstructures
class BlackVarianceSurfaceMoneyness : public LazyObject, public BlackVarianceTermStructure {
public:
    /*! Moneyness can be defined here as spot moneyness, i.e. K/S
     * or forward moneyness, ie K/F
     */
    BlackVarianceSurfaceMoneyness(const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times,
                                  const std::vector<Real>& moneyness,
                                  const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix,
                                  const DayCounter& dayCounter, bool stickyStrike);

    //! \name TermStructure interface
    //@{
    DayCounter dayCounter() const { return dayCounter_; }
    Date maxDate() const { return Date::maxDate(); }
    //@}
    //! \name VolatilityTermStructure interface
    //@{
    Real minStrike() const { return 0; }
    Real maxStrike() const { return QL_MAX_REAL; }
    //@}
    //! \name Observer interface
    //@{
    void update();
    //@}
    //! \name LazyObject interface
    //@{
    void performCalculations() const;
    //@}
    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&);
    //@}
protected:
    virtual Real moneyness(Time t, Real strike) const = 0;
    bool stickyStrike_;
    Handle<Quote> spot_;
    std::vector<Time> times_;

private:
    Real blackVarianceMoneyness(Time t, Real moneyness) const;
    virtual Real blackVarianceImpl(Time t, Real strike) const;
    DayCounter dayCounter_;
    Date maxDate_;
    std::vector<Real> moneyness_;
    std::vector<std::vector<Handle<Quote> > > quotes_;
    mutable Matrix variances_;
    mutable Interpolation2D varianceSurface_;
};

// inline definitions

inline void BlackVarianceSurfaceMoneyness::accept(AcyclicVisitor& v) {
    Visitor<BlackVarianceSurfaceMoneyness>* v1 = dynamic_cast<Visitor<BlackVarianceSurfaceMoneyness>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        BlackVarianceTermStructure::accept(v);
}

//! Black volatility surface based on spot moneyness
//!  \ingroup termstructures
class BlackVarianceSurfaceMoneynessSpot : public BlackVarianceSurfaceMoneyness {
public:
    /*! Moneyness is defined here as spot moneyness, i.e. K/S */

    BlackVarianceSurfaceMoneynessSpot(const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times,
                                      const std::vector<Real>& moneyness,
                                      const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix,
                                      const DayCounter& dayCounter, bool stickyStrike = false);

private:
    virtual Real moneyness(Time t, Real strike) const;
};

//! Black volatility surface based on forward moneyness
//! \ingroup termstructures
class BlackVarianceSurfaceMoneynessForward : public BlackVarianceSurfaceMoneyness {
public:
    /*! Moneyness is defined here as forward moneyness, ie K/F */
    BlackVarianceSurfaceMoneynessForward(const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times,
                                         const std::vector<Real>& moneyness,
                                         const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix,
                                         const DayCounter& dayCounter, const Handle<YieldTermStructure>& forTS,
                                         const Handle<YieldTermStructure>& domTS, bool stickyStrike = false);

private:
    virtual Real moneyness(Time t, Real strike) const;
    Handle<YieldTermStructure> forTS_; // calculates fwd if StickyStrike==false
    Handle<YieldTermStructure> domTS_;
    std::vector<Real> forwards_; // cache fwd values if StickyStrike==true
    Interpolation forwardCurve_;
};

} // namespace QuantExt

#endif

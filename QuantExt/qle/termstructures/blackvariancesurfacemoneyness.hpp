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

#include <ql/math/interpolation.hpp>
#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Abstract Black volatility surface based on moneyness (moneyness defined in subclasses)
/*! \todo times should not be in the interface here. There should be a Dates based constructor and a Periods
          based constructor. This would cover the cases of fixed expiry options and options specified in terms
          of tenors. The times should be calculated internally in the class. What is maxDate() when you use times
          in the interface?

    \ingroup termstructures
*/
class BlackVarianceSurfaceMoneyness : public LazyObject, public BlackVarianceTermStructure {
public:
    /*! Moneyness can be defined here as spot moneyness, i.e. K/S
     * or forward moneyness, ie K/F
     */
    BlackVarianceSurfaceMoneyness(const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times,
                                  const std::vector<Real>& moneyness,
                                  const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix,
                                  const DayCounter& dayCounter, bool stickyStrike, bool flatExtrapMoneyness = false, 
                                  BlackVolTimeExtrapolation timeExtrapolation = BlackVolTimeExtrapolation::FlatInVolatility);

    //! Moneyness variance surface with a fixed reference date.
    BlackVarianceSurfaceMoneyness(const Date& referenceDate, const Calendar& cal, const Handle<Quote>& spot,
                                  const std::vector<Time>& times, const std::vector<Real>& moneyness,
                                  const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix,
                                  const DayCounter& dayCounter, bool stickyStrike, bool flatExtrapMoneyness = false, 
                                  BlackVolTimeExtrapolation timeExtrapolation = BlackVolTimeExtrapolation::FlatInVolatility);

    //! \name TermStructure interface
    //@{
    Date maxDate() const override { return Date::maxDate(); }
    //@}
    //! \name VolatilityTermStructure interface
    //@{
    Real minStrike() const override { return 0; }
    Real maxStrike() const override { return QL_MAX_REAL; }
    //@}
    //! \name Observer interface
    //@{
    void update() override;
    //@}
    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}
    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}

    //! \name Inspectors
    //@{
    std::vector<Real> moneyness() const { return moneyness_; }
    //@}

protected:
    virtual Real moneyness(Time t, Real strike) const = 0;
    bool stickyStrike_;
    Handle<Quote> spot_;
    std::vector<Time> times_;
    std::vector<Real> moneyness_;
    bool flatExtrapMoneyness_;
    BlackVolTimeExtrapolation timeExtrapolation_;

private:
    // Shared initialisation
    void init();

    Real blackVarianceMoneyness(Time t, Real moneyness) const;
    virtual Real blackVarianceImpl(Time t, Real strike) const override;
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
                                      const DayCounter& dayCounter, bool stickyStrike = false,
                                      bool flatExtrapMoneyness = false, 
                                      BlackVolTimeExtrapolation timeExtrapolation = BlackVolTimeExtrapolation::FlatInVolatility);

    //! Spot moneyness variance surface with a fixed reference date.
    BlackVarianceSurfaceMoneynessSpot(const Date& referenceDate, const Calendar& cal, const Handle<Quote>& spot,
                                      const std::vector<Time>& times, const std::vector<Real>& moneyness,
                                      const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix,
                                      const DayCounter& dayCounter, bool stickyStrike = false,
                                      bool flatExtrapMoneyness = false, 
                                      BlackVolTimeExtrapolation timeExtrapolation = BlackVolTimeExtrapolation::FlatInVolatility);

private:
    virtual Real moneyness(Time t, Real strike) const override;
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
                                         const Handle<YieldTermStructure>& domTS, bool stickyStrike = false,
                                         bool flatExtrapMoneyness = false, 
                                         BlackVolTimeExtrapolation timeExtrapolation = BlackVolTimeExtrapolation::FlatInVolatility);

    //! Forward moneyness variance surface with a fixed reference date.
    BlackVarianceSurfaceMoneynessForward(const Date& referenceDate, const Calendar& cal, const Handle<Quote>& spot,
                                         const std::vector<Time>& times, const std::vector<Real>& moneyness,
                                         const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix,
                                         const DayCounter& dayCounter, const Handle<YieldTermStructure>& forTS,
                                         const Handle<YieldTermStructure>& domTS, bool stickyStrike = false,
                                         bool flatExtrapMoneyness = false, 
                                         BlackVolTimeExtrapolation timeExtrapolation = BlackVolTimeExtrapolation::FlatInVolatility);

private:
    // Shared initialisation
    void init();

    virtual Real moneyness(Time t, Real strike) const override;
    Handle<YieldTermStructure> forTS_; // calculates fwd if StickyStrike==false
    Handle<YieldTermStructure> domTS_;
    std::vector<Real> forwards_; // cache fwd values if StickyStrike==true
    QuantLib::Interpolation forwardCurve_;
};

} // namespace QuantExt

#endif

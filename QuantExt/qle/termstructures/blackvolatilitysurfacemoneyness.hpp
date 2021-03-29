/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

/*! \file blackvolatilitysurfacemoneyness.hpp
 \brief Black volatility surface based on forward moneyness interpolated on volatility
 \ingroup termstructures
 */

#ifndef quantext_black_volatility_surface_moneyness_hpp
#define quantext_black_volatility_surface_moneyness_hpp

#include <ql/math/interpolation.hpp>
#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

/*! Abstract Black volatility surface based on moneyness (moneyness defined in subclasses), 
    interpolated on volatility instead of variance.
*/
/*! \todo times should not be in the interface here. There should be a Dates based constructor and a Periods
          based constructor. This would cover the cases of fixed expiry options and options specified in terms
          of tenors. The times should be calculated internally in the class. What is maxDate() when you use times
          in the interface?

    \ingroup termstructures
*/
class BlackVolatilitySurfaceMoneyness : public LazyObject, public BlackVolatilityTermStructure {
public:
    /*! Moneyness can be defined here as spot moneyness, i.e. K/S
     * or forward moneyness, ie K/F
     */
    BlackVolatilitySurfaceMoneyness(const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times,
                                  const std::vector<Real>& moneyness,
                                  const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix,
                                  const DayCounter& dayCounter, bool stickyStrike, bool flatExtrapMoneyness = false);

    //! Moneyness volatility surface with a fixed reference date.
    BlackVolatilitySurfaceMoneyness(const Date& referenceDate, const Calendar& cal, const Handle<Quote>& spot,
                                  const std::vector<Time>& times, const std::vector<Real>& moneyness,
                                  const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix,
                                  const DayCounter& dayCounter, bool stickyStrike, bool flatExtrapMoneyness = false);

    //! \name TermStructure interface
    //@{
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

    //! \name Inspectors
    //@{
    std::vector<QuantLib::Real> moneyness() const { return moneyness_; }
    //@}

protected:
    virtual Real moneyness(Time t, Real strike) const = 0;
    bool stickyStrike_;
    Handle<Quote> spot_;
    std::vector<Time> times_;
    std::vector<Real> moneyness_;
    bool flatExtrapMoneyness_;

private:
    // Shared initialisation
    void init();

    Real blackVarianceMoneyness(Time t, Real moneyness) const;
    virtual Volatility blackVolImpl(Time t, Real strike) const;
    std::vector<std::vector<Handle<Quote> > > quotes_;
    mutable Matrix volatilities_;
    mutable Interpolation2D volatilitySurface_;
};

// inline definitions

inline void BlackVolatilitySurfaceMoneyness::accept(AcyclicVisitor& v) {
    Visitor<BlackVolatilitySurfaceMoneyness>* v1 = dynamic_cast<Visitor<BlackVolatilitySurfaceMoneyness>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        BlackVolatilityTermStructure::accept(v);
}

//! Black volatility surface based on spot moneyness
//!  \ingroup termstructures
class BlackVolatilitySurfaceMoneynessSpot : public BlackVolatilitySurfaceMoneyness {
public:
    /*! Moneyness is defined here as spot moneyness, i.e. K/S */

    BlackVolatilitySurfaceMoneynessSpot(const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times,
                                        const std::vector<Real>& moneyness,
                                        const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix,
                                        const DayCounter& dayCounter, bool stickyStrike = false,
                                        bool flatExtrapMoneyness = false);

    //! Spot moneyness volatility surface with a fixed reference date.
    BlackVolatilitySurfaceMoneynessSpot(const Date& referenceDate, const Calendar& cal, const Handle<Quote>& spot,
                                        const std::vector<Time>& times, const std::vector<Real>& moneyness,
                                        const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix,
                                        const DayCounter& dayCounter, bool stickyStrike = false,
                                        bool flatExtrapMoneyness = false);

private:
    virtual Real moneyness(Time t, Real strike) const;
};

//! Black volatility surface based on forward moneyness
//! \ingroup termstructures
class BlackVolatilitySurfaceMoneynessForward : public BlackVolatilitySurfaceMoneyness {
public:
    /*! Moneyness is defined here as forward moneyness, i.e. K/F */

    BlackVolatilitySurfaceMoneynessForward(const Calendar& cal, const Handle<Quote>& spot,
                                           const std::vector<Time>& times, const std::vector<Real>& moneyness,
                                           const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix,
                                           const DayCounter& dayCounter, const Handle<YieldTermStructure>& forTS,
                                           const Handle<YieldTermStructure>& domTS, bool stickyStrike = false,
                                           bool flatExtrapMoneyness = false);

    //! Forward moneyness volatility surface with a fixed reference date.
    BlackVolatilitySurfaceMoneynessForward(const Date& referenceDate, const Calendar& cal, const Handle<Quote>& spot,
                                           const std::vector<Time>& times, const std::vector<Real>& moneyness,
                                           const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix,
                                           const DayCounter& dayCounter, const Handle<YieldTermStructure>& forTS,
                                           const Handle<YieldTermStructure>& domTS, bool stickyStrike = false,
                                           bool flatExtrapMoneyness = false);

private:
    // Shared initialisation
    void init();

    virtual Real moneyness(Time t, Real strike) const;
    Handle<YieldTermStructure> forTS_; // calculates fwd if StickyStrike==false
    Handle<YieldTermStructure> domTS_;
    std::vector<Real> forwards_; // cache fwd values if StickyStrike==true
    QuantLib::Interpolation forwardCurve_;
};

} // namespace QuantExt

#endif

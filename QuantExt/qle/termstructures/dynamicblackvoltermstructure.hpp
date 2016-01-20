/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file dynamicblackvoltermstructure.hpp
    \brief dynamic black volatility term structure
*/

#ifndef quantext_dynamic_black_volatility_termstructure_hpp
#define quantext_dynamic_black_volatility_termstructure_hpp

#include <qle/math/flatextrapolation.hpp>

#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

/*! This class takes a BlackVolTermStructure with fixed reference date
    and turns it into a floating reference date term structure.
    There are different ways of reacting to time decay that can be
    specified. As an additional feature, the class will return the
    ATM volatility if a null strike is given (currently, for this
    extrapolation must be alloed, since there is a check in
    VolatilityTermStructure we can no extend or bypass). ATM is
    defined as the forward level here (which is of particular
    interest for FX term structures). */

class DynamicBlackVolTermStructure : public BlackVolTermStructure {
  public:
    enum ReactionToTimeDecay { KeepVarianceConstant, ForwardForwardVariance };
    enum Stickyness { StickyStrike, StickyLogMoneyness };

    /* For a stickyness that involves ATM calculations, the yield term
       structures and the spot (as of today, i.e. without settlement lag)
       must be given. They are also required if an ATM volatility with null
       strike is requested. The termstructures are expected to have a
       floating reference date consistent with the spot.
       Since we have to store the initial forward curve at construction,
       we sample it on a grid that can be customized here, too. The curve
       is then linearly interpolated and extrapolated flat after the
       last grid point. */

    DynamicBlackVolTermStructure(
        const Handle<BlackVolTermStructure> &source, Natural settlementDays,
        const Calendar &calendar,
        ReactionToTimeDecay decayMode = ForwardForwardVariance,
        Stickyness stickyness = StickyStrike,
        const Handle<YieldTermStructure> &riskfree =
            Handle<YieldTermStructure>(),
        const Handle<YieldTermStructure> &dividend =
            Handle<YieldTermStructure>(),
        const Handle<Quote> &spot = Handle<Quote>(),
        const std::vector<Real> initialForwardGrid = std::vector<Real>());

    Real atm() const;

  protected:
    /* BlackVolTermStructure interface */
    Real blackVarianceImpl(Time t, Real strike) const;
    Volatility blackVolImpl(Time t, Real strike) const;
    /* VolatilityTermStructure interface */
    Real minStrike() const;
    Real maxStrike() const;
    /* TermStructure interface */
    Date maxDate() const;
    /* Observer interface */
    void update();

  private:
    const Handle<BlackVolTermStructure> &source_;
    ReactionToTimeDecay decayMode_;
    Stickyness stickyness_;
    const Handle<YieldTermStructure> riskfree_, dividend_;
    const Handle<Quote> spot_;
    const Date originalReferenceDate_;
    const bool atmKnown_;
    std::vector<Real> forwardCurveSampleGrid_, initialForwards_;
    boost::shared_ptr<Interpolation> initialForwardCurve_;
};

} // namespace QuantExt

#endif

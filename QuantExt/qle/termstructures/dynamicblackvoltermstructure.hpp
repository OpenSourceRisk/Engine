/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file termstructures/dynamicblackvoltermstructure.hpp
    \brief dynamic black volatility term structure
    \ingroup termstructures
*/

#ifndef quantext_dynamic_black_volatility_termstructure_hpp
#define quantext_dynamic_black_volatility_termstructure_hpp

#include <qle/math/flatextrapolation.hpp>
#include <qle/termstructures/dynamicstype.hpp>

#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

namespace tag {
struct curve {};
struct surface {};
} // namespace tag

//! Takes a BlackVolTermStructure with fixed reference date and turns it into a floating reference date term structure.
/*! This class takes a BlackVolTermStructure with fixed reference date
    and turns it into a floating reference date term structure.
    There are different ways of reacting to time decay that can be
    specified. As an additional feature, the class will return the
    ATM volatility if a null strike is given (currently, for this
    extrapolation must be allowed, since there is a check in
    VolatilityTermStructure we can no extend or bypass). ATM is
    defined as the forward level here (which is of particular
    interest for FX term structures).

    if curve is specified, a more efficient implementation for
    variance and volatility is used just passing through the
    given strike to the source term structure; note that in this
    case a null strike will not be converted to atm though.

        \ingroup termstructures
*/
template <typename mode = tag::surface> class DynamicBlackVolTermStructure : public BlackVolTermStructure {
public:
    /* For a stickyness that involves ATM calculations, the yield term
       structures and the spot (as of today, i.e. without settlement lag)
       must be given. They are also required if an ATM volatility with null
       strike is requested. The termstructures are expected to have a
       floating reference date consistent with the spot.
       Since we have to store the initial forward curve at construction,
       we sample it on a grid that can be customized here, too. The curve
       is then linearly interpolated and extrapolated flat after the
       last grid point. */

    DynamicBlackVolTermStructure(const Handle<BlackVolTermStructure>& source, Natural settlementDays,
                                 const Calendar& calendar, ReactionToTimeDecay decayMode = ConstantVariance,
                                 Stickyness stickyness = StickyLogMoneyness,
                                 const Handle<YieldTermStructure>& riskfree = Handle<YieldTermStructure>(),
                                 const Handle<YieldTermStructure>& dividend = Handle<YieldTermStructure>(),
                                 const Handle<Quote>& spot = Handle<Quote>(),
                                 const std::vector<Real> initialForwardGrid = std::vector<Real>());

    Real atm() const;

    /* VolatilityTermStructure interface */
    Real minStrike() const;
    Real maxStrike() const;
    /* TermStructure interface */
    Date maxDate() const;
    /* Observer interface */
    void update();

protected:
    /* BlackVolTermStructure interface */
    Real blackVarianceImpl(Time t, Real strike) const;
    Volatility blackVolImpl(Time t, Real strike) const;
    /* immplementations for curve and surface tags */
    Real blackVarianceImplTag(Time t, Real strike, tag::curve) const;
    Real blackVarianceImplTag(Time t, Real strike, tag::surface) const;

private:
    const Handle<BlackVolTermStructure> source_;
    ReactionToTimeDecay decayMode_;
    Stickyness stickyness_;
    const Handle<YieldTermStructure> riskfree_, dividend_;
    const Handle<Quote> spot_;
    const Date originalReferenceDate_;
    const bool atmKnown_;
    std::vector<Real> forwardCurveSampleGrid_, initialForwards_;
    boost::shared_ptr<Interpolation> initialForwardCurve_;
};

template <typename mode>
DynamicBlackVolTermStructure<mode>::DynamicBlackVolTermStructure(const Handle<BlackVolTermStructure>& source,
                                                                 Natural settlementDays, const Calendar& calendar,
                                                                 ReactionToTimeDecay decayMode, Stickyness stickyness,
                                                                 const Handle<YieldTermStructure>& riskfree,
                                                                 const Handle<YieldTermStructure>& dividend,
                                                                 const Handle<Quote>& spot,
                                                                 const std::vector<Real> forwardCurveSampleGrid)
    : BlackVolTermStructure(settlementDays, calendar, source->businessDayConvention(), source->dayCounter()),
      source_(source), decayMode_(decayMode), stickyness_(stickyness), riskfree_(riskfree), dividend_(dividend),
      spot_(spot), originalReferenceDate_(source->referenceDate()),
      atmKnown_(!riskfree.empty() && !dividend.empty() && !spot.empty()),
      forwardCurveSampleGrid_(forwardCurveSampleGrid) {

    QL_REQUIRE(stickyness == StickyStrike || stickyness == StickyLogMoneyness,
               "stickyness (" << stickyness << ") not supported");
    QL_REQUIRE(decayMode == ConstantVariance || decayMode == ForwardForwardVariance,
               "reaction to time decay (" << decayMode << ") not supported");

    registerWith(source);

    if (stickyness != StickyStrike) {
        QL_REQUIRE(atmKnown_, "for stickyness other than strike, the term "
                              "structures and spot must be given");
        QL_REQUIRE(riskfree_->referenceDate() == source_->referenceDate(),
                   "at construction time the reference dates of the volatility "
                   "term structure ("
                       << source->referenceDate() << ") and the risk free yield term structure ("
                       << riskfree_->referenceDate() << ") must be the same");
        QL_REQUIRE(dividend_->referenceDate() == source_->referenceDate(),
                   "at construction time the reference dates of the volatility "
                   "term structure ("
                       << source->referenceDate() << ") and the dividend term structure (" << riskfree_->referenceDate()
                       << ") must be the same");
        registerWith(riskfree_);
        registerWith(dividend_);
        registerWith(spot_);
    }

    if (atmKnown_) {
        if (forwardCurveSampleGrid_.size() == 0) {
            // use default grid
            Real tmp[] = { 0.0, 0.25, 0.5,  0.75, 1.0,  2.0,  3.0,  4.0,  5.0,  6.0,  7.0,
                           8.0, 9.0,  10.0, 12.0, 15.0, 20.0, 25.0, 30.0, 40.0, 50.0, 60.0 };
            forwardCurveSampleGrid_ = std::vector<Real>(tmp, tmp + sizeof(tmp) / sizeof(tmp[0]));
        }
        QL_REQUIRE(close_enough(forwardCurveSampleGrid_[0], 0.0),
                   "forward curve sample grid must start at 0 (" << forwardCurveSampleGrid_[0]);
        initialForwards_.resize(forwardCurveSampleGrid_.size());
        for (Size i = 1; i < forwardCurveSampleGrid_.size(); ++i) {
            QL_REQUIRE(forwardCurveSampleGrid_[i] > forwardCurveSampleGrid_[i - 1],
                       "forward curve sample grid must have increasing times (at "
                           << (i - 1) << ", " << i << ": " << forwardCurveSampleGrid_[i - 1] << ", "
                           << forwardCurveSampleGrid_[i]);
        }
        for (Size i = 0; i < forwardCurveSampleGrid_.size(); ++i) {
            Real t = forwardCurveSampleGrid_[i];
            initialForwards_[i] = spot_->value() / riskfree_->discount(t) * dividend_->discount(t);
        }
        initialForwardCurve_ = boost::make_shared<FlatExtrapolation>(boost::make_shared<LinearInterpolation>(
            forwardCurveSampleGrid_.begin(), forwardCurveSampleGrid_.end(), initialForwards_.begin()));
        initialForwardCurve_->enableExtrapolation();
    }
}

template <typename mode> void DynamicBlackVolTermStructure<mode>::update() { BlackVolTermStructure::update(); }

template <typename mode> Date DynamicBlackVolTermStructure<mode>::maxDate() const {
    if (decayMode_ == ForwardForwardVariance) {
        return source_->maxDate();
    }
    if (decayMode_ == ConstantVariance) {
        return Date(std::min(Date::maxDate().serialNumber(), referenceDate().serialNumber() -
                                                                 originalReferenceDate_.serialNumber() +
                                                                 source_->maxDate().serialNumber()));
    }
    QL_FAIL("unexpected decay mode (" << decayMode_ << ")");
}

template <typename mode> Real DynamicBlackVolTermStructure<mode>::minStrike() const {
    if (stickyness_ == StickyStrike) {
        return source_->minStrike();
    }
    if (stickyness_ == StickyLogMoneyness) {
        // we do not specify this, since it is maturity dependent
        // instead we allow for extrapolation when asking the
        // source for a volatility and are not in sticky strike mode
        return 0.0;
    }
    QL_FAIL("unexpected stickyness (" << stickyness_ << ")");
}

template <typename mode> Real DynamicBlackVolTermStructure<mode>::maxStrike() const {
    if (stickyness_ == StickyStrike) {
        return source_->maxStrike();
    }
    if (stickyness_ == StickyLogMoneyness) {
        // see above
        return QL_MAX_REAL;
    }
    QL_FAIL("unexpected stickyness (" << stickyness_ << ")");
}

template <typename mode> Volatility DynamicBlackVolTermStructure<mode>::blackVolImpl(Time t, Real strike) const {
    Real tmp = std::max(1.0E-6, t);
    return std::sqrt(blackVarianceImpl(tmp, strike) / tmp);
}

template <typename mode> Real DynamicBlackVolTermStructure<mode>::blackVarianceImpl(Time t, Real strike) const {
    return blackVarianceImplTag(t, strike, mode());
}

template <typename mode>
Real DynamicBlackVolTermStructure<mode>::blackVarianceImplTag(Time t, Real strike, tag::surface) const {
    if (strike == Null<Real>()) {
        QL_REQUIRE(atmKnown_, "can not calculate atm level (null strike is "
                              "given) because a curve or the spot is missing");
        strike = spot_->value() / riskfree_->discount(t) * dividend_->discount(t);
    }
    Real scenarioT0 = 0.0, scenarioT1 = t;
    Real scenarioStrike0 = strike, scenarioStrike1 = strike;
    if (decayMode_ == ForwardForwardVariance) {
        scenarioT0 = source_->timeFromReference(referenceDate());
        scenarioT1 = scenarioT0 + t;
    }
    if (stickyness_ == StickyLogMoneyness) {
        Real forward = spot_->value() / riskfree_->discount(t) * dividend_->discount(t);
        scenarioStrike1 = initialForwardCurve_->operator()(scenarioT1) / forward * strike;
        scenarioStrike0 = initialForwardCurve_->operator()(scenarioT0) / spot_->value() * strike;
    }
    return source_->blackVariance(scenarioT1, scenarioStrike1, true) -
           source_->blackVariance(scenarioT0, scenarioStrike0, true);
}

template <typename mode>
Real DynamicBlackVolTermStructure<mode>::blackVarianceImplTag(Time t, Real strike, tag::curve) const {
    if (decayMode_ == ForwardForwardVariance) {
        Real scenarioT0 = source_->timeFromReference(referenceDate());
        Real scenarioT1 = scenarioT0 + t;
        return source_->blackVariance(scenarioT1, strike, true) - source_->blackVariance(scenarioT0, strike, true);
    } else {
        return source_->blackVariance(t, strike, true);
    }
}

} // namespace QuantExt

#endif

/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/termstructures/dynamicblackvoltermstructure.hpp>

namespace QuantExt {

DynamicBlackVolTermStructure::DynamicBlackVolTermStructure(
    const Handle<BlackVolTermStructure> &source, Natural settlementDays,
    const Calendar &calendar, ReactionToTimeDecay decayMode,
    Stickyness stickyness, const Handle<YieldTermStructure> &riskfree,
    const Handle<YieldTermStructure> &dividend, const Handle<Quote> &spot,
    const std::vector<Real> forwardCurveSampleGrid)
    : BlackVolTermStructure(settlementDays, calendar,
                            source->businessDayConvention(),
                            source->dayCounter()),
      source_(source), decayMode_(decayMode), stickyness_(stickyness),
      riskfree_(riskfree), dividend_(dividend), spot_(spot),
      originalReferenceDate_(source->referenceDate()),
      atmKnown_(!riskfree.empty() && !dividend.empty() && !spot.empty()),
      forwardCurveSampleGrid_(forwardCurveSampleGrid) {

    QL_REQUIRE(stickyness == StickyStrike || stickyness == StickyLogMoneyness,
               "unknown stickyness (" << stickyness << ")");
    QL_REQUIRE(decayMode == KeepVarianceConstant ||
                   decayMode == ForwardForwardVariance,
               "unknown reaction to time decay (" << stickyness << ")");

    registerWith(source);

    if (stickyness != StickyStrike) {
        QL_REQUIRE(atmKnown_, "for stickyness other than strike, the term "
                              "structures and spot must be given");
        QL_REQUIRE(riskfree_->referenceDate() == source_->referenceDate(),
                   "at construction time the reference dates of the volatility "
                   "term structure ("
                       << source->referenceDate()
                       << ") and the risk free yield term structure ("
                       << riskfree_->referenceDate() << ") must be the same");
        QL_REQUIRE(dividend_->referenceDate() == source_->referenceDate(),
                   "at construction time the reference dates of the volatility "
                   "term structure ("
                       << source->referenceDate()
                       << ") and the dividend term structure ("
                       << riskfree_->referenceDate() << ") must be the same");
        registerWith(riskfree_);
        registerWith(dividend_);
        registerWith(spot_);
    }

    if (atmKnown_) {
        if (forwardCurveSampleGrid_.size() == 0) {
            // use default grid
            Real tmp[] = {0.0,  0.25, 0.5,  0.75, 1.0,  2.0,  3.0,  4.0,
                          5.0,  6.0,  7.0,  8.0,  9.0,  10.0, 12.0, 15.0,
                          20.0, 25.0, 30.0, 40.0, 50.0, 60.0};
            forwardCurveSampleGrid_ =
                std::vector<Real>(tmp, tmp + sizeof(tmp) / sizeof(tmp[0]));
        }
        QL_REQUIRE(close_enough(forwardCurveSampleGrid_[0], 0.0),
                   "forward curve sample grid must start at 0 ("
                       << forwardCurveSampleGrid_[0]);
        initialForwards_.resize(forwardCurveSampleGrid_.size());
        for (Size i = 1; i < forwardCurveSampleGrid_.size(); ++i) {
            QL_REQUIRE(
                forwardCurveSampleGrid_[i] > forwardCurveSampleGrid_[i - 1],
                "forward curve sample grid must have increasing times (at "
                    << (i - 1) << ", " << i << ": "
                    << forwardCurveSampleGrid_[i - 1] << ", "
                    << forwardCurveSampleGrid_[i]);
        }
        for (Size i = 0; i < forwardCurveSampleGrid_.size(); ++i) {
            Real t = forwardCurveSampleGrid_[i];
            initialForwards_[i] = spot_->value() / riskfree_->discount(t) *
                                  dividend_->discount(t);
        }
        initialForwardCurve_ = boost::make_shared<FlatExtrapolation>(
            boost::make_shared<LinearInterpolation>(
                forwardCurveSampleGrid_.begin(), forwardCurveSampleGrid_.end(),
                initialForwards_.begin()));
        initialForwardCurve_->enableExtrapolation();
    }
}

void DynamicBlackVolTermStructure::update() { BlackVolTermStructure::update(); }

Date DynamicBlackVolTermStructure::maxDate() const {
    if (decayMode_ == ForwardForwardVariance) {
        return source_->maxDate();
    }
    if (decayMode_ == KeepVarianceConstant) {
        return Date(std::min(Date::maxDate().serialNumber(),
                             referenceDate().serialNumber() -
                                 originalReferenceDate_.serialNumber() +
                                 source_->maxDate().serialNumber()));
    }
    QL_FAIL("unexpected decay mode (" << decayMode_ << ")");
}

Real DynamicBlackVolTermStructure::minStrike() const {
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

Real DynamicBlackVolTermStructure::maxStrike() const {
    if (stickyness_ == StickyStrike) {
        return source_->maxStrike();
    }
    if (stickyness_ == StickyLogMoneyness) {
        // see above
        return QL_MAX_REAL;
    }
    QL_FAIL("unexpected stickyness (" << stickyness_ << ")");
}

Volatility DynamicBlackVolTermStructure::blackVolImpl(Time t,
                                                      Real strike) const {
    Real tmp = std::max(1.0E-6, t);
    return std::sqrt(blackVarianceImpl(tmp, strike) / tmp);
}

Real DynamicBlackVolTermStructure::blackVarianceImpl(Time t,
                                                     Real strike) const {
    if (strike == Null<Real>()) {
        QL_REQUIRE(atmKnown_, "can not calculate atm level (null strike is "
                              "given) because a curve or the spot is missing");
        strike =
            spot_->value() / riskfree_->discount(t) * dividend_->discount(t);
    }
    Real scenarioT0 = 0.0, scenarioT1 = t;
    Real scenarioStrike0 = strike, scenarioStrike1 = strike;
    if (decayMode_ == ForwardForwardVariance) {
        scenarioT0 = source_->timeFromReference(referenceDate());
        scenarioT1 = scenarioT0 + t;
    }
    if (stickyness_ == StickyLogMoneyness) {
        Real forward =
            spot_->value() / riskfree_->discount(t) * dividend_->discount(t);
        scenarioStrike1 =
            initialForwardCurve_->operator()(scenarioT1) / forward * strike;
        scenarioStrike0 = initialForwardCurve_->operator()(scenarioT0) /
                          spot_->value() * strike;
    }
    return source_->blackVariance(scenarioT1, scenarioStrike1, true) -
           source_->blackVariance(scenarioT0, scenarioStrike0, true);
}

} // namespace QuantExt

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

#include <qle/termstructures/spreadedblackvolatilitysurfacemoneyness.hpp>

#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/flatextrapolation2d.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/forwardcurve.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <boost/make_shared.hpp>

using namespace std;

namespace QuantExt {

SpreadedBlackVolatilitySurfaceMoneyness::SpreadedBlackVolatilitySurfaceMoneyness(
    const Handle<BlackVolTermStructure>& referenceVol, const Handle<Quote>& movingSpot, const std::vector<Time>& times,
    const std::vector<Real>& moneyness, const std::vector<std::vector<Handle<Quote>>>& volSpreads,
    const Handle<Quote>& stickySpot, const Handle<YieldTermStructure>& stickyDividendTs,
    const Handle<YieldTermStructure>& stickyRiskFreeTs, const Handle<YieldTermStructure>& movingDividendTs,
    const Handle<YieldTermStructure>& movingRiskFreeTs, bool stickyStrike)
    : BlackVolatilityTermStructure(referenceVol->businessDayConvention(), referenceVol->dayCounter()),
      referenceVol_(referenceVol), movingSpot_(movingSpot), times_(times), moneyness_(moneyness),
      volSpreads_(volSpreads), stickySpot_(stickySpot), stickyDividendTs_(stickyDividendTs),
      stickyRiskFreeTs_(stickyRiskFreeTs), movingDividendTs_(movingDividendTs), movingRiskFreeTs_(movingRiskFreeTs),
      stickyStrike_(stickyStrike) {

    // register with observables

    registerWith(referenceVol_);
    registerWith(movingSpot_);
    registerWith(stickySpot_);

    for (auto const& v : volSpreads_)
        for (auto const& s : v)
            registerWith(s);

    registerWith(stickyDividendTs_);
    registerWith(stickyRiskFreeTs_);
    registerWith(movingDividendTs_);
    registerWith(movingRiskFreeTs_);

    // check our preconditions on the inputs

    QL_REQUIRE(!times_.empty(), "no times given");
    QL_REQUIRE(!moneyness_.empty(), "no moneyness values given");
    QL_REQUIRE(moneyness_.size() == volSpreads_.size(), "mismatch between moneyness vector and vol matrix rows");

    for (auto const& v : volSpreads_) {
        QL_REQUIRE(times_.size() == v.size(), "mismatch between times vector and vol matrix columns");
    }

    for (Size j = 1; j < times_.size(); ++j) {
        QL_REQUIRE(times_[j] > times_[j - 1], "Times must be sorted and unique but found that the "
                                                  << io::ordinal(j) << " time, " << times_[j]
                                                  << ", is not greater than the " << io::ordinal(j - 1) << " time, "
                                                  << times_[j - 1] << ".");
    }

    // add an artificial time if there is only one to make the interpolation work

    if (times_.size() == 1) {
        times_.push_back(times_.back() + 1.0);
        for (auto& v : volSpreads_)
            v.push_back(v.back());
    }

    // add an artificial moneyness if there is only one to make the interpolation work

    if (moneyness_.size() == 1) {
        moneyness_.push_back(moneyness_.back() + 1.0);
        volSpreads_.push_back(volSpreads_.back());
    }

    // create data matrix used for interpolation and the interpolation object

    data_ = Matrix(moneyness_.size(), times_.size(), 0.0);
    volSpreadSurface_ = FlatExtrapolator2D(QuantLib::ext::make_shared<BilinearInterpolation>(
        times_.begin(), times_.end(), moneyness_.begin(), moneyness_.end(), data_));
    volSpreadSurface_.enableExtrapolation();
}

Date SpreadedBlackVolatilitySurfaceMoneyness::maxDate() const { return referenceVol_->maxDate(); }
const Date& SpreadedBlackVolatilitySurfaceMoneyness::referenceDate() const { return referenceVol_->referenceDate(); }
Calendar SpreadedBlackVolatilitySurfaceMoneyness::calendar() const { return referenceVol_->calendar(); }
Natural SpreadedBlackVolatilitySurfaceMoneyness::settlementDays() const { return referenceVol_->settlementDays(); }
Real SpreadedBlackVolatilitySurfaceMoneyness::minStrike() const { return referenceVol_->minStrike(); }
Real SpreadedBlackVolatilitySurfaceMoneyness::maxStrike() const { return referenceVol_->maxStrike(); }

void SpreadedBlackVolatilitySurfaceMoneyness::update() {
    LazyObject::update();
    BlackVolatilityTermStructure::update();
}

const std::vector<QuantLib::Real>& SpreadedBlackVolatilitySurfaceMoneyness::moneyness() const { return moneyness_; }

void SpreadedBlackVolatilitySurfaceMoneyness::performCalculations() const {
    for (Size j = 0; j < data_.columns(); ++j) {
        for (Size i = 0; i < data_.rows(); ++i) {
            data_(i, j) = volSpreads_[i][j]->value();
        }
    }
    volSpreadSurface_.update();
}

Real SpreadedBlackVolatilitySurfaceMoneyness::blackVolImpl(Time t, Real strike) const {
    calculate();
    QL_REQUIRE(!referenceVol_.empty(), "SpreadedBlackVolatilitySurfaceMoneyness: reference vol is empty");
    Real m = moneynessFromStrike(t, strike, false);
    QL_REQUIRE(std::isfinite(m),
               "SpreadedBlackVolatilitySurfaceMoneyness: got invalid moneyness (dynamic reference) at t = "
                   << t << ", strike = " << strike << ": " << m);
    Real effStrike;
    if (stickyStrike_)
        effStrike = strike;
    else {
        effStrike = strikeFromMoneyness(t, m, true);
        QL_REQUIRE(std::isfinite(effStrike),
                   "SpreadedBlackVolatilitySurfaceMoneyness: got invalid strike from moneyness at t = "
                       << t << ", input strike = " << strike << ", moneyness = " << m);
    }
    Real m2 = moneynessFromStrike(t, strike, false);
    QL_REQUIRE(std::isfinite(m2),
               "SpreadedBlackVolatilitySurfaceMoneyness: got invalid moneyness (sticky reference) at t = "
                   << t << ", strike = " << strike << ": " << m2);
    return referenceVol_->blackVol(t, effStrike) + volSpreadSurface_(t, m2);
}

Real SpreadedBlackVolatilitySurfaceMoneynessSpot::moneynessFromStrike(Time t, Real strike,
                                                                      const bool stickyReference) const {
    if (strike == Null<Real>() || close_enough(strike, 0.0)) {
        return 1.0;
    } else {
        QL_REQUIRE(!stickyReference || !stickySpot_.empty(),
                   "SpreadedBlackVolatilitySurfaceMoneynessSpot: stickySpot is empty");
        QL_REQUIRE(stickyReference || !movingSpot_.empty(),
                   "SpreadedBlackVolatilitySurfaceMoneynessSpot: movingSpot is empty");
        return strike / (stickyReference ? stickySpot_->value() : movingSpot_->value());
    }
}

Real SpreadedBlackVolatilitySurfaceMoneynessSpot::strikeFromMoneyness(Time t, Real moneyness,
                                                                      const bool stickyReference) const {
    QL_REQUIRE(!stickyReference || !stickySpot_.empty(),
               "SpreadedBlackVolatilitySurfaceMoneynessSpot: stickySpot is empty");
    QL_REQUIRE(stickyReference || !movingSpot_.empty(),
               "SpreadedBlackVolatilitySurfaceMoneynessSpot: movingSpot is empty");
    return moneyness * (stickyReference ? stickySpot_->value() : movingSpot_->value());
}

Real SpreadedBlackVolatilitySurfaceLogMoneynessSpot::moneynessFromStrike(Time t, Real strike,
                                                                         const bool stickyReference) const {
    if (strike == Null<Real>() || close_enough(strike, 0.0)) {
        return 0.0;
    } else {
        QL_REQUIRE(!stickyReference || !stickySpot_.empty(),
                   "SpreadedBlackVolatilitySurfaceLogMoneynessSpot: stickySpot is empty");
        QL_REQUIRE(stickyReference || !movingSpot_.empty(),
                   "SpreadedBlackVolatilitySurfaceLogMoneynessSpot: movingSpot is empty");
        return std::log(strike / (stickyReference ? stickySpot_->value() : movingSpot_->value()));
    }
}

Real SpreadedBlackVolatilitySurfaceLogMoneynessSpot::strikeFromMoneyness(Time t, Real moneyness,
                                                                         const bool stickyReference) const {
    QL_REQUIRE(!stickyReference || !stickySpot_.empty(),
               "SpreadedBlackVolatilitySurfaceLogMoneynessSpot: stickySpot is empty");
    QL_REQUIRE(stickyReference || !movingSpot_.empty(),
               "SpreadedBlackVolatilitySurfaceLogMoneynessSpot: movingSpot is empty");
    return std::exp(moneyness) * (stickyReference ? stickySpot_->value() : movingSpot_->value());
}

Real SpreadedBlackVolatilitySurfaceMoneynessForward::moneynessFromStrike(Time t, Real strike,
                                                                         const bool stickyReference) const {
    if (strike == Null<Real>() || close_enough(strike, 0.0))
        return 1.0;
    else {
        Real forward;
        if (stickyReference) {
            QL_REQUIRE(!stickySpot_.empty(), "SpreadedBlackVolatilitySurfaceMoneynessForward: stickySpot is empty");
            QL_REQUIRE(!stickyDividendTs_.empty(),
                       "SpreadedBlackVolatilitySurfaceMoneynessForward: stickyDividendTs is empty");
            QL_REQUIRE(!stickyRiskFreeTs_.empty(),
                       "SpreadedBlackVolatilitySurfaceMoneynessForward: stickyRiskFreeTs is empty");
            forward = stickySpot_->value() * stickyDividendTs_->discount(t) / stickyRiskFreeTs_->discount(t);
        } else {
            QL_REQUIRE(!movingSpot_.empty(), "SpreadedBlackVolatilitySurfaceMoneynessForward: movingSpot is empty");
            QL_REQUIRE(!movingDividendTs_.empty(),
                       "SpreadedBlackVolatilitySurfaceMoneynessForward: movingDividendTs is empty");
            QL_REQUIRE(!movingRiskFreeTs_.empty(),
                       "SpreadedBlackVolatilitySurfaceMoneynessForward: mocingRiskFreeTs is empty");
            forward = movingSpot_->value() * movingDividendTs_->discount(t) / movingRiskFreeTs_->discount(t);
        }
        return strike / forward;
    }
}

Real SpreadedBlackVolatilitySurfaceMoneynessForward::strikeFromMoneyness(Time t, Real moneyness,
                                                                         const bool stickyReference) const {
    Real forward;
    if (stickyReference) {
        QL_REQUIRE(!stickySpot_.empty(), "SpreadedBlackVolatilitySurfaceMoneynessForward: stickySpot is empty");
        QL_REQUIRE(!stickyDividendTs_.empty(),
                   "SpreadedBlackVolatilitySurfaceMoneynessForward: stickyDividendTs is empty");
        QL_REQUIRE(!stickyRiskFreeTs_.empty(),
                   "SpreadedBlackVolatilitySurfaceMoneynessForward: stickyRiskFreeTs is empty");
        forward = stickySpot_->value() * stickyDividendTs_->discount(t) / stickyRiskFreeTs_->discount(t);
    } else {
        QL_REQUIRE(!movingSpot_.empty(), "SpreadedBlackVolatilitySurfaceMoneynessForward: movingSpot is empty");
        QL_REQUIRE(!movingDividendTs_.empty(),
                   "SpreadedBlackVolatilitySurfaceMoneynessForward: movingDividendTs is empty");
        QL_REQUIRE(!movingRiskFreeTs_.empty(),
                   "SpreadedBlackVolatilitySurfaceMoneynessForward: mocingRiskFreeTs is empty");
        forward = movingSpot_->value() * movingDividendTs_->discount(t) / movingRiskFreeTs_->discount(t);
    }
    return moneyness * forward;
}

Real SpreadedBlackVolatilitySurfaceLogMoneynessForward::moneynessFromStrike(Time t, Real strike,
                                                                            const bool stickyReference) const {
    if (strike == Null<Real>() || close_enough(strike, 0.0))
        return 0.0;
    else {
        Real forward;
        if (stickyReference) {
            QL_REQUIRE(!stickySpot_.empty(), "SpreadedBlackVolatilitySurfaceLogMoneynessForward: stickySpot is empty");
            QL_REQUIRE(!stickyDividendTs_.empty(),
                       "SpreadedBlackVolatilitySurfaceLogMoneynessForward: stickyDividendTs is empty");
            QL_REQUIRE(!stickyRiskFreeTs_.empty(),
                       "SpreadedBlackVolatilitySurfaceLogMoneynessForward: stickyRiskFreeTs is empty");
            forward = stickySpot_->value() * stickyDividendTs_->discount(t) / stickyRiskFreeTs_->discount(t);
        } else {
            QL_REQUIRE(!movingSpot_.empty(), "SpreadedBlackVolatilitySurfaceLogMoneynessForward: movingSpot is empty");
            QL_REQUIRE(!movingDividendTs_.empty(),
                       "SpreadedBlackVolatilitySurfaceLogMoneynessForward: movingDividendTs is empty");
            QL_REQUIRE(!movingRiskFreeTs_.empty(),
                       "SpreadedBlackVolatilitySurfaceLogMoneynessForward: mocingRiskFreeTs is empty");
            forward = movingSpot_->value() * movingDividendTs_->discount(t) / movingRiskFreeTs_->discount(t);
        }
        return std::log(strike / forward);
    }
}

Real SpreadedBlackVolatilitySurfaceLogMoneynessForward::strikeFromMoneyness(Time t, Real moneyness,
                                                                            const bool stickyReference) const {
    Real forward;
    if (stickyReference) {
        QL_REQUIRE(!stickySpot_.empty(), "SpreadedBlackVolatilitySurfaceLogMoneynessForward: stickySpot is empty");
        QL_REQUIRE(!stickyDividendTs_.empty(),
                   "SpreadedBlackVolatilitySurfaceLogMoneynessForward: stickyDividendTs is empty");
        QL_REQUIRE(!stickyRiskFreeTs_.empty(),
                   "SpreadedBlackVolatilitySurfaceLogMoneynessForward: stickyRiskFreeTs is empty");
        forward = stickySpot_->value() * stickyDividendTs_->discount(t) / stickyRiskFreeTs_->discount(t);
    } else {
        QL_REQUIRE(!movingSpot_.empty(), "SpreadedBlackVolatilitySurfaceLogMoneynessForward: movingSpot is empty");
        QL_REQUIRE(!movingDividendTs_.empty(),
                   "SpreadedBlackVolatilitySurfaceLogMoneynessForward: movingDividendTs is empty");
        QL_REQUIRE(!movingRiskFreeTs_.empty(),
                   "SpreadedBlackVolatilitySurfaceLogMoneynessForward: mocingRiskFreeTs is empty");
        forward = movingSpot_->value() * movingDividendTs_->discount(t) / movingRiskFreeTs_->discount(t);
    }
    return std::exp(moneyness) * forward;
}

Real SpreadedBlackVolatilitySurfaceStdDevs::moneynessFromStrike(Time t, Real strike, const bool stickyReference) const {
    if (strike == Null<Real>() || close_enough(strike, 0.0) || close_enough(t, 0.0))
        return 0.0;
    else {
        QL_REQUIRE(!stickySpot_.empty(), "SpreadedBlackVolatilitySurfaceStdDevs: stickySpot is empty");
        QL_REQUIRE(!stickyDividendTs_.empty(), "SpreadedBlackVolatilitySurfaceStdDevs: stickyDividendTs is empty");
        QL_REQUIRE(!stickyRiskFreeTs_.empty(), "SpreadedBlackVolatilitySurfaceStdDevs: stickyRiskFreeTs is empty");
        Real stickyForward = stickySpot_->value() * stickyDividendTs_->discount(t) / stickyRiskFreeTs_->discount(t);
        Real forward;
        if (stickyReference) {
            forward = stickyForward;
        } else {
            QL_REQUIRE(!movingSpot_.empty(), "SpreadedBlackVolatilitySurfaceStdDevs: movingSpot is empty");
            QL_REQUIRE(!movingDividendTs_.empty(), "SpreadedBlackVolatilitySurfaceStdDevs: movingDividendTs is empty");
            QL_REQUIRE(!movingRiskFreeTs_.empty(), "SpreadedBlackVolatilitySurfaceStdDevs: mocingRiskFreeTs is empty");
            forward = movingSpot_->value() * movingDividendTs_->discount(t) / movingRiskFreeTs_->discount(t);
        }
        // We use the sticky forward to read the vol for the definition of the standardised moneyness to
        // avoid that this changes under forward curve changes. In the end this is a matter of definition and
        // we might want to revise this later.
        Real vol = referenceVol_->blackVol(t, stickyForward);
        return std::log(strike / forward) / (vol * std::sqrt(t));
    }
}

Real SpreadedBlackVolatilitySurfaceStdDevs::strikeFromMoneyness(Time t, Real moneyness,
                                                                const bool stickyReference) const {
    Real stickyForward = stickySpot_->value() * stickyDividendTs_->discount(t) / stickyRiskFreeTs_->discount(t);
    Real forward;
    if (stickyReference) {
        forward = stickyForward;
    } else {
        QL_REQUIRE(!movingSpot_.empty(), "SpreadedBlackVolatilitySurfaceStdDevs: movingSpot is empty");
        QL_REQUIRE(!movingDividendTs_.empty(), "SpreadedBlackVolatilitySurfaceStdDevs: movingDividendTs is empty");
        QL_REQUIRE(!movingRiskFreeTs_.empty(), "SpreadedBlackVolatilitySurfaceStdDevs: mocingRiskFreeTs is empty");
        forward = movingSpot_->value() * movingDividendTs_->discount(t) / movingRiskFreeTs_->discount(t);
    }
    // We use the sticky forward to read the vol for the definition of the standardised moneyness to
    // avoid that this changes under forward curve changes. In the end this is a matter of definition and
    // we might want to revise this later.
    Real vol = referenceVol_->blackVol(t, stickyForward);
    return std::exp(moneyness * vol * std::sqrt(t)) * forward;
}

Real SpreadedBlackVolatilitySurfaceMoneynessSpotAbsolute::moneynessFromStrike(Time t, Real strike,
                                                                              const bool stickyReference) const {
    if (strike == Null<Real>() || close_enough(strike, 0.0)) {
        return 0.0;
    } else {
        QL_REQUIRE(!stickyReference || !stickySpot_.empty(),
                   "SpreadedBlackVolatilitySurfaceMoneynessSpot: stickySpot is empty");
        QL_REQUIRE(stickyReference || !movingSpot_.empty(),
                   "SpreadedBlackVolatilitySurfaceMoneynessSpot: movingSpot is empty");
        return strike - (stickyReference ? stickySpot_->value() : movingSpot_->value());
    }
}

Real SpreadedBlackVolatilitySurfaceMoneynessSpotAbsolute::strikeFromMoneyness(Time t, Real moneyness,
                                                                              const bool stickyReference) const {
    QL_REQUIRE(!stickyReference || !stickySpot_.empty(),
               "SpreadedBlackVolatilitySurfaceMoneynessSpot: stickySpot is empty");
    QL_REQUIRE(stickyReference || !movingSpot_.empty(),
               "SpreadedBlackVolatilitySurfaceMoneynessSpot: movingSpot is empty");
    return moneyness + (stickyReference ? stickySpot_->value() : movingSpot_->value());
}

Real SpreadedBlackVolatilitySurfaceMoneynessForwardAbsolute::moneynessFromStrike(Time t, Real strike,
                                                                                 const bool stickyReference) const {
    if (strike == Null<Real>() || close_enough(strike, 0.0))
        return 0.0;
    else {
        Real forward;
        if (stickyReference) {
            QL_REQUIRE(!stickySpot_.empty(), "SpreadedBlackVolatilitySurfaceMoneynessForward: stickySpot is empty");
            QL_REQUIRE(!stickyDividendTs_.empty(),
                       "SpreadedBlackVolatilitySurfaceMoneynessForward: stickyDividendTs is empty");
            QL_REQUIRE(!stickyRiskFreeTs_.empty(),
                       "SpreadedBlackVolatilitySurfaceMoneynessForward: stickyRiskFreeTs is empty");
            forward = stickySpot_->value() * stickyDividendTs_->discount(t) / stickyRiskFreeTs_->discount(t);
        } else {
            QL_REQUIRE(!movingSpot_.empty(), "SpreadedBlackVolatilitySurfaceMoneynessForward: movingSpot is empty");
            QL_REQUIRE(!movingDividendTs_.empty(),
                       "SpreadedBlackVolatilitySurfaceMoneynessForward: movingDividendTs is empty");
            QL_REQUIRE(!movingRiskFreeTs_.empty(),
                       "SpreadedBlackVolatilitySurfaceMoneynessForward: mocingRiskFreeTs is empty");
            forward = movingSpot_->value() * movingDividendTs_->discount(t) / movingRiskFreeTs_->discount(t);
        }
        return strike - forward;
    }
}

Real SpreadedBlackVolatilitySurfaceMoneynessForwardAbsolute::strikeFromMoneyness(Time t, Real moneyness,
                                                                                 const bool stickyReference) const {
    Real forward;
    if (stickyReference) {
        QL_REQUIRE(!stickySpot_.empty(), "SpreadedBlackVolatilitySurfaceMoneynessForward: stickySpot is empty");
        QL_REQUIRE(!stickyDividendTs_.empty(),
                   "SpreadedBlackVolatilitySurfaceMoneynessForward: stickyDividendTs is empty");
        QL_REQUIRE(!stickyRiskFreeTs_.empty(),
                   "SpreadedBlackVolatilitySurfaceMoneynessForward: stickyRiskFreeTs is empty");
        forward = stickySpot_->value() * stickyDividendTs_->discount(t) / stickyRiskFreeTs_->discount(t);
    } else {
        QL_REQUIRE(!movingSpot_.empty(), "SpreadedBlackVolatilitySurfaceMoneynessForward: movingSpot is empty");
        QL_REQUIRE(!movingDividendTs_.empty(),
                   "SpreadedBlackVolatilitySurfaceMoneynessForward: movingDividendTs is empty");
        QL_REQUIRE(!movingRiskFreeTs_.empty(),
                   "SpreadedBlackVolatilitySurfaceMoneynessForward: mocingRiskFreeTs is empty");
        forward = movingSpot_->value() * movingDividendTs_->discount(t) / movingRiskFreeTs_->discount(t);
    }
    return moneyness + forward;
}

} // namespace QuantExt

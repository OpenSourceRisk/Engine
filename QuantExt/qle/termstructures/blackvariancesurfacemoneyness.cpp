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
#include <boost/make_shared.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/forwardcurve.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <qle/termstructures/blackvariancesurfacemoneyness.hpp>

using namespace std;

namespace QuantExt {

BlackVarianceSurfaceMoneyness::BlackVarianceSurfaceMoneyness(
    const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times, const std::vector<Real>& moneyness,
    const std::vector<std::vector<Handle<Quote>>>& blackVolMatrix, const DayCounter& dayCounter, bool stickyStrike,
    bool flatExtrapMoneyness, BlackVolTimeExtrapolation timeExtrapolation)
    : BlackVarianceTermStructure(0, cal, Following, dayCounter), stickyStrike_(stickyStrike), spot_(spot),
      times_(times), moneyness_(moneyness), flatExtrapMoneyness_(flatExtrapMoneyness),
      timeExtrapolation_(timeExtrapolation), quotes_(blackVolMatrix) {
    init();
}

BlackVarianceSurfaceMoneyness::BlackVarianceSurfaceMoneyness(
    const Date& referenceDate, const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times,
    const std::vector<Real>& moneyness, const std::vector<std::vector<Handle<Quote>>>& blackVolMatrix,
    const DayCounter& dayCounter, bool stickyStrike, bool flatExtrapMoneyness,
    BlackVolTimeExtrapolation timeExtrapolation)
    : BlackVarianceTermStructure(referenceDate, cal, Following, dayCounter), stickyStrike_(stickyStrike), spot_(spot),
      times_(times), moneyness_(moneyness), flatExtrapMoneyness_(flatExtrapMoneyness),
      timeExtrapolation_(timeExtrapolation), quotes_(blackVolMatrix) {
    init();
}

void BlackVarianceSurfaceMoneyness::update() {
    TermStructure::update();
    LazyObject::update();
}

void BlackVarianceSurfaceMoneyness::performCalculations() const {
    for (Size j = 1; j < variances_.columns(); j++) {
        for (Size i = 0; i < variances_.rows(); i++) {
            Real vol = quotes_[i][j - 1]->value();
            variances_[i][j] = times_[j] * vol * vol;
        }
    }
    varianceSurface_.update();
}

void BlackVarianceSurfaceMoneyness::init() {

    QL_REQUIRE(times_.size() == quotes_.front().size(), "mismatch between times vector and vol matrix columns");
    QL_REQUIRE(moneyness_.size() == quotes_.size(), "mismatch between moneyness vector and vol matrix rows");

    QL_REQUIRE(times_[0] > 0, "The first time must be greater than 0.0.");

    if (stickyStrike_) {
        // we don't want to know if the spot has changed - we take a copy here
        spot_ = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(spot_->value()));
    } else {
        registerWith(spot_);
    }

    // Insert time 0.0 in times_ and initialise variances_ with 0.0.
    times_.insert(times_.begin(), 0.0);
    variances_ = Matrix(moneyness_.size(), times_.size(), 0.0);

    // Check times_ and register with quotes
    for (Size j = 1; j < times_.size(); j++) {

        QL_REQUIRE(times_[j] > times_[j - 1], "Times must be sorted and unique but found that the "
                                                  << io::ordinal(j) << " time, " << times_[j]
                                                  << ", is not greater than the " << io::ordinal(j - 1) << " time, "
                                                  << times_[j - 1] << ".");

        for (Size i = 0; i < moneyness_.size(); i++) {
            registerWith(quotes_[i][j - 1]);
        }
    }

    varianceSurface_ =
        Bilinear().interpolate(times_.begin(), times_.end(), moneyness_.begin(), moneyness_.end(), variances_);

    notifyObservers();
}

Real BlackVarianceSurfaceMoneyness::blackVarianceImpl(Time t, Real strike) const {

    calculate();

    if (t == 0.0)
        return 0.0;

    return std::max(blackVarianceMoneyness(t, moneyness(t, strike)), 0.0);
}

Real BlackVarianceSurfaceMoneyness::blackVarianceMoneyness(Time t, Real m) const {
    if (t <= times_.back() || timeExtrapolation_ == BlackVolTimeExtrapolation::UseInterpolator) {
        return varianceSurface_(t, m, true);
    } else if (t > times_.back() && timeExtrapolation_ == BlackVolTimeExtrapolation::FlatInVolatility) {
        return varianceSurface_(times_.back(), m, true) * t / times_.back();
    } else if (t > times_.back() && timeExtrapolation_ == BlackVolTimeExtrapolation::LinearInVolatility) {
        Size ind1 = times_.size() - 2;
        Size ind2 = times_.size() - 1;
        std::array<Real, 2> times;
        times[0] = times_[ind1];
        times[1] = times_[ind2];
        std::array<Real, 2> vols;
        vols[0] = close_enough(times[0], 0.0) ? 0.0 : std::sqrt(varianceSurface_(times[0], m, true) / times[0]);
        vols[1] = close_enough(times[1], 0.0) ? 0.0 : std::sqrt(varianceSurface_(times[1], m, true) / times[1]);
        LinearInterpolation interpolation(times.begin(), times.end(), vols.begin());
        Real v = interpolation(t);
        return v * v * t;
    } else {
        QL_FAIL("Unknown time extrapolation method");
    }
}

BlackVarianceSurfaceMoneynessSpot::BlackVarianceSurfaceMoneynessSpot(
    const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times, const std::vector<Real>& moneyness,
    const std::vector<std::vector<Handle<Quote>>>& blackVolMatrix, const DayCounter& dayCounter, bool stickyStrike,
    bool flatExtrapMoneyness, BlackVolTimeExtrapolation timeExtrapolation)
    : BlackVarianceSurfaceMoneyness(cal, spot, times, moneyness, blackVolMatrix, dayCounter, stickyStrike,
                                    flatExtrapMoneyness, timeExtrapolation) {}

BlackVarianceSurfaceMoneynessSpot::BlackVarianceSurfaceMoneynessSpot(
    const Date& referenceDate, const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times,
    const std::vector<Real>& moneyness, const std::vector<std::vector<Handle<Quote>>>& blackVolMatrix,
    const DayCounter& dayCounter, bool stickyStrike, bool flatExtrapMoneyness, BlackVolTimeExtrapolation timeExtrapolation)
    : BlackVarianceSurfaceMoneyness(referenceDate, cal, spot, times, moneyness, blackVolMatrix, dayCounter,
                                    stickyStrike, flatExtrapMoneyness, timeExtrapolation) {}

Real BlackVarianceSurfaceMoneynessSpot::moneyness(Time, Real strike) const {
    if (strike == Null<Real>() || strike == 0) {
        return 1.0;
    } else {
        Real moneyness = strike / spot_->value();
        if (flatExtrapMoneyness_) {
            if (moneyness < moneyness_.front()) {
                moneyness = moneyness_.front();
            } else if (moneyness > moneyness_.back()) {
                moneyness = moneyness_.back();
            }
        }
        return moneyness;
    }
}

BlackVarianceSurfaceMoneynessForward::BlackVarianceSurfaceMoneynessForward(
    const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times, const std::vector<Real>& moneyness,
    const std::vector<std::vector<Handle<Quote>>>& blackVolMatrix, const DayCounter& dayCounter,
    const Handle<YieldTermStructure>& forTS, const Handle<YieldTermStructure>& domTS, bool stickyStrike,
    bool flatExtrapMoneyness, BlackVolTimeExtrapolation timeExtrapolation)
    : BlackVarianceSurfaceMoneyness(cal, spot, times, moneyness, blackVolMatrix, dayCounter, stickyStrike,
                                    flatExtrapMoneyness, timeExtrapolation),
      forTS_(forTS), domTS_(domTS) {
    init();
}

BlackVarianceSurfaceMoneynessForward::BlackVarianceSurfaceMoneynessForward(
    const Date& referenceDate, const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times,
    const std::vector<Real>& moneyness, const std::vector<std::vector<Handle<Quote>>>& blackVolMatrix,
    const DayCounter& dayCounter, const Handle<YieldTermStructure>& forTS, const Handle<YieldTermStructure>& domTS,
    bool stickyStrike, bool flatExtrapMoneyness, BlackVolTimeExtrapolation timeExtrapolation)
    : BlackVarianceSurfaceMoneyness(referenceDate, cal, spot, times, moneyness, blackVolMatrix, dayCounter,
                                    stickyStrike, flatExtrapMoneyness, timeExtrapolation),
      forTS_(forTS), domTS_(domTS) {
    init();
}

void BlackVarianceSurfaceMoneynessForward::init() {

    if (!stickyStrike_) {
        QL_REQUIRE(!forTS_.empty(), "foreign discount curve required for atmf surface");
        QL_REQUIRE(!domTS_.empty(), "domestic discount curve required for atmf surface");
        registerWith(forTS_);
        registerWith(domTS_);
    } else {
        for (Size i = 0; i < times_.size(); i++) {
            Time t = times_[i];
            Real fwd = spot_->value() * forTS_->discount(t) / domTS_->discount(t);
            forwards_.push_back(fwd);
        }
        forwardCurve_ = Linear().interpolate(times_.begin(), times_.end(), forwards_.begin());
    }
}

Real BlackVarianceSurfaceMoneynessForward::moneyness(Time t, Real strike) const {
    Real fwd;
    Real reqMoneyness; // for flat extrapolation
    if (strike == Null<Real>() || strike == 0)
        return 1.0;
    else {
        if (stickyStrike_)
            fwd = forwardCurve_(t, true);
        else
            fwd = spot_->value() * forTS_->discount(t) / domTS_->discount(t);
        reqMoneyness = strike / fwd;
        if (flatExtrapMoneyness_) {
            if ((strike / fwd) < moneyness_.front()) {
                reqMoneyness = moneyness_.front();
            } else if ((strike / fwd) > moneyness_.back()) {
                reqMoneyness = moneyness_.back();
            }
        }
        return reqMoneyness;
    }
}

} // namespace QuantExt

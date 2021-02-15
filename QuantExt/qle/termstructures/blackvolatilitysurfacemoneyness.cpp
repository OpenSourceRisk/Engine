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
#include <boost/make_shared.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/forwardcurve.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <qle/termstructures/blackvolatilitysurfacemoneyness.hpp>

using namespace std;

namespace QuantExt {

BlackVolatilitySurfaceMoneyness::BlackVolatilitySurfaceMoneyness(
    const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times, const std::vector<Real>& moneyness,
    const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix, const DayCounter& dayCounter, bool stickyStrike,
    bool flatExtrapMoneyness)
    : BlackVolatilityTermStructure(0, cal, Following, dayCounter), stickyStrike_(stickyStrike), spot_(spot),
      times_(times), moneyness_(moneyness), flatExtrapMoneyness_(flatExtrapMoneyness), quotes_(blackVolMatrix) {
    init();
}

BlackVolatilitySurfaceMoneyness::BlackVolatilitySurfaceMoneyness(
    const Date& referenceDate, const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times,
    const std::vector<Real>& moneyness, const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix,
    const DayCounter& dayCounter, bool stickyStrike, bool flatExtrapMoneyness)
    : BlackVolatilityTermStructure(referenceDate, cal, Following, dayCounter), stickyStrike_(stickyStrike), spot_(spot),
      times_(times), moneyness_(moneyness), flatExtrapMoneyness_(flatExtrapMoneyness), quotes_(blackVolMatrix) {
    init();
}

void BlackVolatilitySurfaceMoneyness::update() {
    TermStructure::update();
    LazyObject::update();
}

void BlackVolatilitySurfaceMoneyness::performCalculations() const {
    for (Size j = 1; j < volatilities_.columns(); j++) {
        for (Size i = 0; i < volatilities_.rows(); i++) {
            Real vol = quotes_[i][j - 1]->value();
            volatilities_[i][j] = vol;
        }
    }
    volatilitySurface_.update();
}

void BlackVolatilitySurfaceMoneyness::init() {
    QL_REQUIRE(times_.size() == quotes_.front().size(), "mismatch between times vector and vol matrix colums");
    QL_REQUIRE(moneyness_.size() == quotes_.size(), "mismatch between moneyness vector and vol matrix rows");

    QL_REQUIRE(times_[0] > 0, "The first time must be greater than 0.0.");

    if (stickyStrike_) {
        // we don't want to know if the spot has changed - we take a copy here
        spot_ = Handle<Quote>(boost::make_shared<SimpleQuote>(spot_->value()));
    } else {
        registerWith(spot_);
    }

    // Insert time 0.0 in times_ and initialise volatilities_ with 0.0.
    times_.insert(times_.begin(), 0.0);
    volatilities_ = Matrix(moneyness_.size(), times_.size(), 0.0);

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

    volatilitySurface_ =
        Bilinear().interpolate(times_.begin(), times_.end(), moneyness_.begin(), moneyness_.end(), volatilities_);

    notifyObservers();
}

Volatility BlackVolatilitySurfaceMoneyness::blackVolImpl(Time t, Real strike) const {
    calculate();

    if (t == 0.0)
        return 0.0;

    return std::sqrt(blackVarianceMoneyness(t, moneyness(t, strike)) / t);
}

Real BlackVolatilitySurfaceMoneyness::blackVarianceMoneyness(Time t, Real m) const {
    Real vol = 0.0;
    if (t <= times_.back()) {
        vol = volatilitySurface_(t, m, true);
    } else { // I.e. if t > times_.back(), extrapolate from back()
        vol = volatilitySurface_(times_.back(), m, true);
    }
    return vol * vol * t;
}

BlackVolatilitySurfaceMoneynessSpot::BlackVolatilitySurfaceMoneynessSpot(
    const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times, const std::vector<Real>& moneyness,
    const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix, const DayCounter& dayCounter, bool stickyStrike,
    bool flatExtrapMoneyness)
    : BlackVolatilitySurfaceMoneyness(cal, spot, times, moneyness, blackVolMatrix, dayCounter, stickyStrike,
                                      flatExtrapMoneyness) {}

BlackVolatilitySurfaceMoneynessSpot::BlackVolatilitySurfaceMoneynessSpot(
    const Date& referenceDate, const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times,
    const std::vector<Real>& moneyness, const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix,
    const DayCounter& dayCounter, bool stickyStrike, bool flatExtrapMoneyness)
    : BlackVolatilitySurfaceMoneyness(referenceDate, cal, spot, times, moneyness, blackVolMatrix, dayCounter,
                                      stickyStrike, flatExtrapMoneyness) {}

Real BlackVolatilitySurfaceMoneynessSpot::moneyness(Time, Real strike) const {
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

BlackVolatilitySurfaceMoneynessForward::BlackVolatilitySurfaceMoneynessForward(
    const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times, const std::vector<Real>& moneyness,
    const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix, const DayCounter& dayCounter,
    const Handle<YieldTermStructure>& forTS, const Handle<YieldTermStructure>& domTS, bool stickyStrike,
    bool flatExtrapMoneyness)
    : BlackVolatilitySurfaceMoneyness(cal, spot, times, moneyness, blackVolMatrix, dayCounter, stickyStrike,
                                      flatExtrapMoneyness),
      forTS_(forTS), domTS_(domTS) {
    init();
}

BlackVolatilitySurfaceMoneynessForward::BlackVolatilitySurfaceMoneynessForward(
    const Date& referenceDate, const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times,
    const std::vector<Real>& moneyness, const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix,
    const DayCounter& dayCounter, const Handle<YieldTermStructure>& forTS, const Handle<YieldTermStructure>& domTS,
    bool stickyStrike, bool flatExtrapMoneyness)
    : BlackVolatilitySurfaceMoneyness(referenceDate, cal, spot, times, moneyness, blackVolMatrix, dayCounter,
                                      stickyStrike, flatExtrapMoneyness),
      forTS_(forTS), domTS_(domTS) {
    init();
}

void BlackVolatilitySurfaceMoneynessForward::init() {
    if (!stickyStrike_) {
        QL_REQUIRE(!forTS_.empty(), "foreign discount curve required for moneyness forward surface");
        QL_REQUIRE(!domTS_.empty(), "domestic discount curve required for moneyness forward surface");
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

Real BlackVolatilitySurfaceMoneynessForward::moneyness(Time t, Real strike) const {
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

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
    const Handle<BlackVolTermStructure>& referenceVol, const Handle<Quote>& spot, const std::vector<Time>& times,
    const std::vector<Real>& moneyness, const std::vector<std::vector<Handle<Quote>>>& volSpreads, bool stickyStrike)
    : BlackVolatilityTermStructure(referenceVol->businessDayConvention(), referenceVol->dayCounter()),
      referenceVol_(referenceVol), spot_(spot), times_(times), moneyness_(moneyness), volSpreads_(volSpreads),
      stickyStrike_(stickyStrike) {
    init();
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

void SpreadedBlackVolatilitySurfaceMoneyness::performCalculations() const {
    for (Size j = 0; j < data_.columns(); ++j) {
        for (Size i = 0; i < data_.rows(); ++i) {
            data_(i, j) = volSpreads_[i][j]->value();
        }
    }
    volSpreadSurface_.update();
}

void SpreadedBlackVolatilitySurfaceMoneyness::init() {

    QL_REQUIRE(times_.size() == volSpreads_.front().size(), "mismatch between times vector and vol matrix colums");
    QL_REQUIRE(moneyness_.size() == volSpreads_.size(), "mismatch between moneyness vector and vol matrix rows");

    if (stickyStrike_) {
        // we don't want to know if the spot has changed - we take a copy here
        spot_ = Handle<Quote>(boost::make_shared<SimpleQuote>(spot_->value()));
    } else {
        registerWith(spot_);
    }

    data_ = Matrix(moneyness_.size(), times_.size(), 0.0);

    for (Size j = 0; j < times_.size(); ++j) {
        QL_REQUIRE(times_[j] > times_[j - 1], "Times must be sorted and unique but found that the "
                                                  << io::ordinal(j) << " time, " << times_[j]
                                                  << ", is not greater than the " << io::ordinal(j - 1) << " time, "
                                                  << times_[j - 1] << ".");
    }

    for (Size j = 0; j < times_.size(); ++j) {
        for (Size i = 0; i < moneyness_.size(); i++) {
            registerWith(volSpreads_[i][j]);
        }
    }

    volSpreadSurface_ = FlatExtrapolator2D(boost::make_shared<BilinearInterpolation>(
        times_.begin(), times_.end(), moneyness_.begin(), moneyness_.end(), data_));
    volSpreadSurface_.enableExtrapolation();
}

Real SpreadedBlackVolatilitySurfaceMoneyness::blackVolImpl(Time t, Real strike) const {
    calculate();
    return referenceVol_->blackVol(t, strike) + volSpreadSurface_(t, moneyness(t, strike));
}

SpreadedBlackVolatilitySurfaceMoneynessSpot::SpreadedBlackVolatilitySurfaceMoneynessSpot(
    const Handle<BlackVolTermStructure>& referenceVol, const Handle<Quote>& spot, const std::vector<Time>& times,
    const std::vector<Real>& moneyness, const std::vector<std::vector<Handle<Quote>>>& volSpreads, bool stickyStrike)
    : SpreadedBlackVolatilitySurfaceMoneyness(referenceVol, spot, times, moneyness, volSpreads, stickyStrike) {}

Real SpreadedBlackVolatilitySurfaceMoneynessSpot::moneyness(Time, Real strike) const {
    if (strike == Null<Real>() || strike == 0) {
        return 1.0;
    } else {
        return strike / spot_->value();
    }
}

SpreadedBlackVolatilitySurfaceMoneynessForward::SpreadedBlackVolatilitySurfaceMoneynessForward(
    const Handle<BlackVolTermStructure>& referenceVol, const Handle<Quote>& spot, const std::vector<Time>& times,
    const std::vector<Real>& moneyness, const std::vector<std::vector<Handle<Quote>>>& volSpreads,
    const Handle<YieldTermStructure>& forTS, const Handle<YieldTermStructure>& domTS, bool stickyStrike)
    : SpreadedBlackVolatilitySurfaceMoneyness(referenceVol, spot, times, moneyness, volSpreads, stickyStrike),
      forTS_(forTS), domTS_(domTS) {
    init();
}

void SpreadedBlackVolatilitySurfaceMoneynessForward::init() {

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

Real SpreadedBlackVolatilitySurfaceMoneynessForward::moneyness(Time t, Real strike) const {
    if (strike == Null<Real>() || strike == 0)
        return 1.0;
    else {
        Real fwd;
        if (stickyStrike_)
            fwd = forwardCurve_(t, true);
        else
            fwd = spot_->value() * forTS_->discount(t) / domTS_->discount(t);
        return strike / fwd;
    }
}

} // namespace QuantExt

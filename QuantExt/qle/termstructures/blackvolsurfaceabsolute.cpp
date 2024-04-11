/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/termstructures/blackvolsurfaceabsolute.hpp>
#include <qle/math/flatextrapolation.hpp>

#include <ql/experimental/fx/blackdeltacalculator.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/interpolations/cubicinterpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/pricingengines/blackformula.hpp>

namespace QuantExt {

BlackVolatilitySurfaceAbsolute::BlackVolatilitySurfaceAbsolute(
    Date referenceDate, const std::vector<Date>& dates, const std::vector<std::vector<Real>>& strikes,
    const std::vector<std::vector<Real>>& strikeQuotes, const DayCounter& dayCounter, const Calendar& calendar,
    const Handle<Quote>& spot, const Size spotDays, const Calendar spotCalendar,
    const Handle<YieldTermStructure>& domesticTS, const Handle<YieldTermStructure>& foreignTS,
    const DeltaVolQuote::DeltaType dt, const DeltaVolQuote::AtmType at, const Period& switchTenor,
    const DeltaVolQuote::DeltaType ltdt, const DeltaVolQuote::AtmType ltat, const SmileInterpolation smileInterpolation,
    const bool flatExtrapolation)
    : BlackVolatilityTermStructure(referenceDate, calendar, Following, dayCounter), dates_(dates), strikes_(strikes),
      strikeQuotes_(strikeQuotes), spot_(spot), spotDays_(spotDays), spotCalendar_(spotCalendar),
      domesticTS_(domesticTS), foreignTS_(foreignTS), dt_(dt), at_(at), switchTenor_(switchTenor), ltdt_(ltdt),
      ltat_(ltat), smileInterpolation_(smileInterpolation), flatExtrapolation_(flatExtrapolation) {

    // checks

    QL_REQUIRE(!dates_.empty(), "BlackVolatilitySurfaceAbsolute: no expiry dates given");
    QL_REQUIRE(!strikes_.empty(), "BlackVolatilitySurfaceAbsolute: no strikes given");

    for (Size i = 0; i < strikes_.size(); ++i) {
        if (strikes_[i].size() > 1) {
            for (Size j = 0; j < strikes_[i].size() - 1; ++j) {
                QL_REQUIRE(strikes_[i][j + 1] > strikes_[i][j] && !close_enough(strikes_[i][j + 1], strikes_[i][j]),
                           "BlackVolatilitySurfaceAbsolute: strikes are not strictly ascending at index "
                               << i << ", " << j
                               << ": " << strikes_[i][j] << ", " << strikes_[i][j + 1]);
            }
        }
    }

    QL_REQUIRE(strikeQuotes_.size() == dates_.size(), "BlackVolatilitySurfaceAbsolute: strikeQuotes ("
                                                      << strikeQuotes_.size() << ") mismatch with expiry dates ("
                                                      << dates_.size() << ")");
    QL_REQUIRE(strikeQuotes_.size() == strikes_.size(), "BlackVolatilitySurfaceAbsolute: strikeQuotes ("
                                                      << strikeQuotes_.size() << ") mismatch with number of dates in strikes ("
                                                      << strikes_.size() << ")");
    for (Size i = 0; i < strikes_.size(); ++i) {
        QL_REQUIRE(strikeQuotes_[i].size() == strikes_[i].size(), "BlackVolatilitySurfaceAbsolute: strikeQuotes inner vector ("
                       << strikeQuotes_[i].size() << ") mismatch with strikes (" << strikes_[i].size() << ")");
    }

    // calculate times associated to expiry dates

    expiryTimes_.clear();
    settlementDates_.clear();
    for (auto const& d : dates_) {
        expiryTimes_.push_back(timeFromReference(d));
        settlementDates_.push_back(spotCalendar_.advance(d, spotDays_ * Days));
    }

    // generate interpolator
    interpolation_.resize(strikeQuotes_.size());
    for (Size i = 0; i < strikeQuotes_.size(); ++i) {
        if (strikes_[i].size() > 1) {
            if (smileInterpolation_ == BlackVolatilitySurfaceAbsolute::SmileInterpolation::Linear) {
                interpolation_[i] = QuantLib::ext::make_shared<LinearInterpolation>(strikes_[i].begin(), strikes_[i].end(),
                                                                            strikeQuotes_[i].begin());
                interpolation_[i]->enableExtrapolation();
            } else if (smileInterpolation_ == BlackVolatilitySurfaceAbsolute::SmileInterpolation::Cubic) {
                interpolation_[i] = QuantLib::ext::make_shared<CubicInterpolation>(strikes_[i].begin(), strikes_[i].end(), strikeQuotes_[i].begin(), CubicInterpolation::Spline, false,
                    CubicInterpolation::SecondDerivative, 0.0, CubicInterpolation::SecondDerivative, 0.0);
                interpolation_[i]->enableExtrapolation();
            } else {
                QL_FAIL("BlackVolatilitySurfaceAbsolute: Invalid interpolation type.");
            }
        }
        if (flatExtrapolation_) {
            interpolation_[i] = QuantLib::ext::make_shared<FlatExtrapolation>(interpolation_[i]);
            interpolation_[i]->enableExtrapolation();
        }
    }

    // register with observables

    registerWith(spot_);
    registerWith(domesticTS_);
    registerWith(foreignTS_);
}

void BlackVolatilitySurfaceAbsolute::update() {
    BlackVolatilityTermStructure::update();
}

Volatility BlackVolatilitySurfaceAbsolute::blackVolImpl(Time t, Real strike) const {

    /* minimum supported time is 1D, i.e. if t is smaller, we return the vol at 1D */

    t = std::max(t, 1.0 / 365.0);

    t = t <= expiryTimes_.back() ? t : expiryTimes_.back();

    /* if we have cached the interpolated smile at t, we use that */

    auto s = cachedInterpolatedVols_.find(std::make_pair(t, strike));
    if (s != cachedInterpolatedVols_.end()) {
        return s->second;
    }

    /* find the indices ip and im such that t_im <= t  < t_ip, im will be null if t < first expiry,
       ip will be null if t >= last expiry */

    Size index_p = std::upper_bound(expiryTimes_.begin(), expiryTimes_.end(), t) - expiryTimes_.begin();
    Size index_m = index_p == 0 ? Null<Size>() : index_p - 1;
    if (index_p == expiryTimes_.size())
        index_p = Null<Size>();

    /* build the smiles on the indices, if we do not have them yet */
    Volatility vol_p = 0, vol_m = 0;
    if (index_p != Null<Size>()) {
        if (strikeQuotes_[index_p].size() == 1) {
            vol_p = strikeQuotes_[index_p][0];
        } else {
            vol_p = (*interpolation_[index_p])(strike);
        }
    }
    if (index_m != Null<Size>()) {
        if (strikeQuotes_[index_m].size() == 1) {
            vol_m = strikeQuotes_[index_m][0];
        } else {
            vol_m = (*interpolation_[index_m])(strike);
        }
    }

    /* find the interpolated vols */

    Real vol;

    if (index_p == Null<Size>()) {
        vol = vol_m;
    } else if (index_m == Null<Size>()) {
        vol = vol_p;
    } else {
        // interpolate between two expiries
        Real a = (t - expiryTimes_[index_m]) / (expiryTimes_[index_p] - expiryTimes_[index_m]);
        vol = (1.0 - a) * vol_m + a * vol_p;
    }

    /* store the new smile in the cache */

    cachedInterpolatedVols_[std::make_pair(t, strike)] = vol;

    return vol;
}

} // namespace QuantExt

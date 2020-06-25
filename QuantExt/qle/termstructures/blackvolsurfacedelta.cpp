/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ql/errors.hpp>
#include <ql/experimental/fx/blackdeltacalculator.hpp>
#include <ql/math/interpolations/cubicinterpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <qle/termstructures/blackvolsurfacedelta.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

InterpolatedSmileSection::InterpolatedSmileSection(Real spot, Real rd, Real rf, Time t,
                                                   const std::vector<Real>& strikes,
                                                   const std::vector<Volatility>& vols, InterpolationMethod method,
                                                   bool flatExtrapolation)
    : FxSmileSection(spot, rd, rf, t), strikes_(strikes), vols_(vols), flatExtrapolation_(flatExtrapolation) {

    if (method == InterpolationMethod::Linear)
        interpolator_ = Linear().interpolate(strikes_.begin(), strikes_.end(), vols_.begin());
    else if (method == InterpolationMethod::NaturalCubic)
        interpolator_ =
            Cubic(CubicInterpolation::Kruger, true).interpolate(strikes_.begin(), strikes_.end(), vols_.begin());
    else if (method == InterpolationMethod::FinancialCubic)
        interpolator_ = Cubic(CubicInterpolation::Kruger, true, CubicInterpolation::SecondDerivative, 0.0,
                              CubicInterpolation::FirstDerivative)
                            .interpolate(strikes_.begin(), strikes_.end(), vols_.begin());
    else {
        QL_FAIL("Invalid method " << (int)method);
    }
}

Volatility InterpolatedSmileSection::volatility(Real strike) const {
    if (flatExtrapolation_) {
        if (strike < strikes_.front())
            return vols_.front();
        else if (strike > strikes_.back())
            return vols_.back();
    }
    // and then call the interpolator with extrapolation on
    return interpolator_(strike, true);
}

BlackVolatilitySurfaceDelta::BlackVolatilitySurfaceDelta(
    Date referenceDate, const std::vector<Date>& dates, const std::vector<Real>& putDeltas,
    const std::vector<Real>& callDeltas, bool hasAtm, const Matrix& blackVolMatrix, const DayCounter& dayCounter,
    const Calendar& cal, const Handle<Quote>& spot, const Handle<YieldTermStructure>& domesticTS,
    const Handle<YieldTermStructure>& foreignTS, DeltaVolQuote::DeltaType dt, DeltaVolQuote::AtmType at,
    boost::optional<DeltaVolQuote::DeltaType> atmDeltaType, InterpolatedSmileSection::InterpolationMethod im,
    bool flatExtrapolation)
    : BlackVolatilityTermStructure(referenceDate, cal, Following, dayCounter), dates_(dates), times_(dates.size(), 0),
      putDeltas_(putDeltas), callDeltas_(callDeltas), hasAtm_(hasAtm), spot_(spot), domesticTS_(domesticTS),
      foreignTS_(foreignTS), dt_(dt), at_(at), atmDeltaType_(atmDeltaType), interpolationMethod_(im),
      flatExtrapolation_(flatExtrapolation) {

    // If ATM delta type is not given, set it to dt
    if (!atmDeltaType_)
        atmDeltaType_ = dt_;

    QL_REQUIRE(dates.size() > 1, "at least 1 date required");
    // this has already been done for dates
    for (Size i = 0; i < dates.size(); i++) {
        QL_REQUIRE(referenceDate < dates[i], "Dates must be greater than reference date");
        times_[i] = timeFromReference(dates[i]);
        if (i > 0) {
            QL_REQUIRE(times_[i] > times_[i - 1], "dates must be sorted unique!");
        }
    }

    // check size of matrix
    Size n = putDeltas.size() + (hasAtm ? 1 : 0) + callDeltas.size();
    QL_REQUIRE(n > 0, "Need at least one delta");
    QL_REQUIRE(blackVolMatrix.columns() == n, "Invalid number of columns in blackVolMatrix, got "
                                                  << blackVolMatrix.columns() << " but have " << n << " deltas");
    QL_REQUIRE(blackVolMatrix.rows() == dates.size(), "Invalid number of rows in blackVolMatrix, got "
                                                          << blackVolMatrix.rows() << " but have " << dates.size()
                                                          << " dates");

    // build interpolators for each delta
    // TODO: template this so it can be changed
    bool forceMonotoneVariance = false;
    for (Size i = 0; i < n; i++) {
        vector<Volatility> vols(dates.size());
        for (Size j = 0; j < dates.size(); j++) {
            vols[j] = blackVolMatrix[j][i];
        }

        // BlackVarianceCurve will make a local copy of vols and dates
        interpolators_.push_back(
            boost::make_shared<BlackVarianceCurve>(referenceDate, dates, vols, dayCounter, forceMonotoneVariance));
    }

    // register
    registerWith(spot_);
    registerWith(domesticTS_);
    registerWith(foreignTS_);
}

boost::shared_ptr<FxSmileSection> BlackVolatilitySurfaceDelta::blackVolSmile(Time t) const {

    Real spot = spot_->value();
    DiscountFactor dDiscount = domesticTS_->discount(t);
    DiscountFactor fDiscount = foreignTS_->discount(t);

    // Store smile section in map. Use strikes as key and vols as values for automatic sorting by strike.
    // If we have already have a strike from a previous delta, we do not overwrite it.
    auto comp = [](Real a, Real b) { return !close(a, b) && a < b; };
    map<Real, Real, decltype(comp)> smileSection(comp);
    Size i = 0;
    for (Real delta : putDeltas_) {
        Real vol = interpolators_.at(i)->blackVol(t, 1, true);
        BlackDeltaCalculator bdc(Option::Put, dt_, spot, dDiscount, fDiscount, vol * sqrt(t));
        Real strike = bdc.strikeFromDelta(delta);
        if (smileSection.count(strike) == 0)
            smileSection[strike] = vol;
        i++;
    }
    if (hasAtm_) {
        Real vol = interpolators_.at(i)->blackVol(t, 1, true);
        BlackDeltaCalculator bdc(Option::Put, *atmDeltaType_, spot, dDiscount, fDiscount, vol * sqrt(t));
        Real strike = bdc.atmStrike(at_);
        if (smileSection.count(strike) == 0)
            smileSection[strike] = vol;
        i++;
    }
    for (Real delta : callDeltas_) {
        Real vol = interpolators_.at(i)->blackVol(t, 1, true);
        BlackDeltaCalculator bdc(Option::Call, dt_, spot, dDiscount, fDiscount, vol * sqrt(t));
        Real strike = bdc.strikeFromDelta(delta);
        if (smileSection.count(strike) == 0)
            smileSection[strike] = vol;
        i++;
    }

    // sort and extract to vectors
    vector<Real> strikes;
    strikes.reserve(smileSection.size());
    vector<Real> vols;
    vols.reserve(smileSection.size());
    for (const auto& kv : smileSection) {
        strikes.push_back(kv.first);
        vols.push_back(kv.second);
    }

    // now build smile from strikes and vols
    QL_REQUIRE(!vols.empty(),
               "BlackVolatilitySurfaceDelta::blackVolSmile(" << t << "): no strikes given, this is unexpected.");
    if (vols.size() == 1) {
        // handle the situation that we only have one strike (might occur for e.g. t=0)
        return boost::make_shared<ConstantSmileSection>(vols.front());
    } else {
        // we have at least two strikes
        return boost::make_shared<InterpolatedSmileSection>(spot, dDiscount, fDiscount, t, strikes, vols,
                                                            interpolationMethod_, flatExtrapolation_);
    }
}

boost::shared_ptr<FxSmileSection> BlackVolatilitySurfaceDelta::blackVolSmile(const Date& d) const {
    return blackVolSmile(timeFromReference(d));
}

Real BlackVolatilitySurfaceDelta::forward(Time t) const {
    return spot_->value() * foreignTS_->discount(t) / domesticTS_->discount(t); // TODO
}

Volatility BlackVolatilitySurfaceDelta::blackVolImpl(Time t, Real strike) const {
    // If asked for strike == 0, just return the ATM value.
    if (strike == 0 || strike == Null<Real>()) {
        if (hasAtm_) {
            // ask the ATM interpolator directly
            return interpolators_[putDeltas_.size()]->blackVol(t, Null<Real>(), true);
        } else {
            // set strike to be fwd and we will return ATMF
            strike = forward(t);
        }
    }
    return blackVolSmile(t)->volatility(strike);
}

} // namespace QuantExt

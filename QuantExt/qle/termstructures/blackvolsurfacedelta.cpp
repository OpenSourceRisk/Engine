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

#include <qle/termstructures/blackvolsurfacedelta.hpp>
#include <ql/experimental/fx/blackdeltacalculator.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/interpolations/cubicinterpolation.hpp>
#include <ql/errors.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

InterpolatedSmileSection::InterpolatedSmileSection
(Real spot, Real rd, Real rf, Time t, const std::vector<Real>& strikes, const std::vector<Volatility>& vols,
 InterpolationMethod method, bool flatExtrapolation)
: FxSmileSection(spot, rd, rf, t), strikes_(strikes), vols_(vols), flatExtrapolation_(flatExtrapolation) {

    if (method == InterpolationMethod::Linear)
        interpolator_ = Linear().interpolate(strikes_.begin(), strikes_.end(), vols_.begin());
    else if (method == InterpolationMethod::NaturalCubic)
        interpolator_ = Cubic(CubicInterpolation::Kruger, true)
            .interpolate(strikes_.begin(), strikes_.end(), vols_.begin());
    else if (method == InterpolationMethod::FinancialCubic)
        interpolator_ = Cubic(CubicInterpolation::Kruger, true, CubicInterpolation::SecondDerivative,
                              0.0, CubicInterpolation::FirstDerivative)
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
    Date referenceDate,
    const std::vector<Date>& dates, const std::vector<Real>& putDeltas, const std::vector<Real>& callDeltas,
    bool hasAtm, const Matrix& blackVolMatrix, const DayCounter& dayCounter, const Calendar& cal,
    const Handle<Quote>& spot, const Handle<YieldTermStructure>& domesticTS,
    const Handle<YieldTermStructure>& foreignTS, DeltaVolQuote::DeltaType dt,
    DeltaVolQuote::AtmType at, InterpolatedSmileSection::InterpolationMethod im, bool flatExtrapolation)
    : BlackVolatilityTermStructure(referenceDate, cal, Following, dayCounter),
      dates_(dates), times_(dates.size(), 0), putDeltas_(putDeltas), callDeltas_(callDeltas), hasAtm_(hasAtm),
      spot_(spot), domesticTS_(domesticTS), foreignTS_(foreignTS), dt_(dt), at_(at), interpolationMethod_(im),
      flatExtrapolation_(flatExtrapolation) {

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
    QL_REQUIRE(blackVolMatrix.columns() == n, "Invalid number of columns in blackVolMatrix, got " <<
               blackVolMatrix.columns() << " but have " << n << " deltas");
    QL_REQUIRE(blackVolMatrix.rows() == dates.size(), "Invalid number of rows in blackVolMatrix, got " <<
               blackVolMatrix.rows() << " but have " << dates.size() << " dates");

    // build interpolators for each delta
    // TODO: template this so it can be changed
    bool forceMonotoneVariance = false;
    for (Size i = 0; i < n; i++) {
        vector<Volatility> vols(dates.size());
        for (Size j = 0; j < dates.size(); j++) {
            vols[j] = blackVolMatrix[j][i];
        }
        
        // BlackVarianceCurve will make a local copy of vols and dates
        interpolators_.push_back(boost::make_shared<BlackVarianceCurve>(referenceDate, dates, vols, dayCounter, forceMonotoneVariance));
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

    // we interpolate on all the curves

    // get vols
    vector<Real> vols;
    for (const auto& interp : interpolators_) {
        vols.push_back(interp->blackVol(t, 1, true));
    }

    // get strikes - need to handle the 3 groups
    // store them in a vector of pairs for now for easy sorting
    vector<pair<Real, Real>> tmp;
    Size i = 0;
    for (Real delta : putDeltas_) {
        BlackDeltaCalculator bdc(Option::Put, dt_, spot, dDiscount, fDiscount, vols[i]);
        tmp.push_back(make_pair(vols[i], bdc.strikeFromDelta(delta)));
        i++;
    }
    if (hasAtm_) {
        BlackDeltaCalculator bdc(Option::Put, dt_, spot, dDiscount, fDiscount, vols[i]);
        tmp.push_back(make_pair(vols[i], bdc.atmStrike(at_)));
        i++;
    }
    for (Real delta : callDeltas_) {
        BlackDeltaCalculator bdc(Option::Call, dt_, spot, dDiscount, fDiscount, vols[i]);
        tmp.push_back(make_pair(vols[i], bdc.strikeFromDelta(delta)));
        i++;
    }
    // sort and extract to vectors
    std::sort(tmp.begin(), tmp.end());
    vector<Real> strikes (vols.size(), 0.0);
    for (Size i = 0; i < tmp.size(); i++) {
        vols[i] = tmp[i].first;
        strikes[i] = tmp[i].second;
    }

    // now build smile from strikes and vols
    return boost::make_shared<InterpolatedSmileSection>(spot, dDiscount, fDiscount, t, strikes, vols, interpolationMethod_, flatExtrapolation_);
}

Real BlackVolatilitySurfaceDelta::forward(Time t) const {
    return spot_->value()* foreignTS_->discount(t) / domesticTS_->discount(t); // TODO
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

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

#include <qle/termstructures/blackvariancesurfacesparse.hpp>
using namespace std;
using namespace QuantLib;

namespace QuantExt {

template <class StrikeInterpolation, class TimeInterpolation>
BlackVarianceSurfaceSparse<StrikeInterpolation, TimeInterpolation>::BlackVarianceSurfaceSparse(const Date& referenceDate, const Calendar& cal,
                                                       const vector<Date>& dates, const vector<Real>& strikes,
                                                       const vector<Volatility>& volatilities,
                                                       const DayCounter& dayCounter, bool lowerStrikeConstExtrap,
                                                       bool upperStrikeConstExtrap,
                                                       QuantLib::BlackVolTimeExtrapolation::Type timeExtrapolationType,
                                                       const VolatilityType volType,
                                                       const Real shift)
    : BlackVarianceTermStructure(referenceDate, cal, Following, dayCounter, volType, shift),
      OptionInterpolator2d<StrikeInterpolation, TimeInterpolation>(referenceDate, dayCounter, lowerStrikeConstExtrap, upperStrikeConstExtrap),
      timeExtrapolationType_(timeExtrapolationType) {

    QL_REQUIRE((strikes.size() == dates.size()) && (dates.size() == volatilities.size()),
               "dates, strikes and volatilities vectors not of equal size.");

    // convert volatilities to variance
    vector<Real> variances(volatilities.size());
    for (Size i = 0; i < volatilities.size(); i++) {
        Time t = dayCounter.yearFraction(referenceDate, dates[i]);
        variances[i] = volatilities[i] * volatilities[i] * t;
    }

    // variance must be 0 at time 0, add a variance of zero, time 0, for 2 strikes to ensure interpolation
    vector<Date> modDates(dates.begin(), dates.end());
    vector<Real> modStrikes(strikes.begin(), strikes.end());
    vector<Real> tempStrikes;
    tempStrikes.push_back(1.0);
    tempStrikes.push_back(100.0);
    for (Size i = 0; i < tempStrikes.size(); i++) {
        modDates.push_back(referenceDate);
        modStrikes.push_back(tempStrikes[i]);
        variances.push_back(0.0);
    }

    this->initialise(modDates, modStrikes, variances);
}

template <class StrikeInterpolation, class TimeInterpolation>
QuantLib::Real BlackVarianceSurfaceSparse<StrikeInterpolation, TimeInterpolation>::blackVarianceImpl(QuantLib::Time t, QuantLib::Real strike) const {
    QuantLib::Time tb = this->times().back();
    if (t <= tb) {
        return this->getValue(t, strike);
    } else
        return BlackVolTimeExtrapolation::extrapolatedVariance(timeExtrapolationType_, t, strike, this->times(),
                                                               [&](Real t, Real k) { return this->getValue(t, k); });
};

template class BlackVarianceSurfaceSparse<QuantLib::Linear, QuantLib::Linear>;
template class BlackVarianceSurfaceSparse<QuantLib::Linear, CubicSpline>;
template class BlackVarianceSurfaceSparse<QuantLib::Linear, BackwardFlat>;
template class BlackVarianceSurfaceSparse<CubicSpline, QuantLib::Linear>;
template class BlackVarianceSurfaceSparse<CubicSpline, CubicSpline>;
template class BlackVarianceSurfaceSparse<CubicSpline, BackwardFlat>;
} // namespace QuantExt

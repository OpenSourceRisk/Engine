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

BlackVarianceSurfaceSparse::BlackVarianceSurfaceSparse(const Date& referenceDate, const Calendar& cal,
                                                       const vector<Date>& dates, const vector<Real>& strikes,
                                                       const vector<Volatility>& volatilities,
                                                       const DayCounter& dayCounter, bool lowerStrikeConstExtrap,
                                                       bool upperStrikeConstExtrap, bool timeFlatExtrapolation)
    : BlackVarianceTermStructure(referenceDate, cal), OptionInterpolator2d<Linear, Linear>(referenceDate, dayCounter,
                                                                                           lowerStrikeConstExtrap,
                                                                                           upperStrikeConstExtrap),
      timeFlatExtrapolation_(timeFlatExtrapolation) {

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

    initialise(modDates, modStrikes, variances);
}

QuantLib::Real BlackVarianceSurfaceSparse::blackVarianceImpl(QuantLib::Time t, QuantLib::Real strike) const {

    QuantLib::Time tb = times().back();
    if (timeFlatExtrapolation_ && t > tb) {
        return getValue(tb, strike) * t / tb;
    } else {
        return getValue(t, strike);
    }
};
} // namespace QuantExt

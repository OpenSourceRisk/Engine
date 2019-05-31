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

#include <boost/make_shared.hpp>
#include <iostream>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/forwardcurve.hpp>
#include <qle/termstructures/blackvariancesurfacesparse.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

BlackVarianceSurfaceSparse::BlackVarianceSurfaceSparse(const Date& referenceDate, const Calendar& cal,
                                                       const vector<Date>& dates, const vector<Real>& strikes,
                                                       const vector<Volatility>& volatilities,
                                                       const DayCounter& dayCounter)
    : BlackVarianceTermStructure(referenceDate, cal) {

    QL_REQUIRE((strikes.size() == dates.size()) && (dates.size() == volatilities.size()),
        "dates, strikes and volatilities vectors not of equal size.");
    
    // convert volatilities to variance
    vector<Real> variances(volatilities.size());
    for (Size i = 0; i < volatilities.size(); i++) {
        Time t = dayCounter.yearFraction(referenceDate, dates[i]);
        variances[i] = volatilities[i] * volatilities[i] * t;
    }

    OptionInterpolator2d(referenceDate, dates, strikes, variances, dayCounter);

}

} // namespace QuantExt
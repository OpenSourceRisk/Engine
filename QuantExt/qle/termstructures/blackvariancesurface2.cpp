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

#include <ql/math/interpolations/cubicinterpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <qle/termstructures/blackvariancesurface2.hpp>

namespace QuantExt {

BlackVarianceSurface2::BlackVarianceSurface2(const Date& referenceDate, const Calendar& cal,
                                             const std::vector<Date>& dates,
                                             const std::vector<std::vector<Real> >& strikes,
                                             const std::vector<std::vector<Volatility> >& vols,
                                             const DayCounter& dayCounter)
    : BlackVarianceTermStructure(referenceDate), dayCounter_(dayCounter), strikes_(strikes), vols_(vols) {

    QL_REQUIRE(dates.size() > 0, "No dates");
    QL_REQUIRE(dates.size() == strikes.size(), "Dates / Strikes size mismatch");
    QL_REQUIRE(dates.size() == vols.size(), "Dates / vols size mismatch");
    QL_REQUIRE(dates[0] >= referenceDate, "cannot have dates[0] < referenceDate");

    for (Size i = 0; i < strikes.size(); i++) {
        QL_REQUIRE(strikes[i].size() == vols[i].size(), "Strikes / Vols size mismatch for " << dates[i]);

        // check strikes are increasing
        for (Size j = i; j < strikes[i].size(); j++) {
            QL_REQUIRE(strikes[i][j - 1] < strikes[i][j], "Strikes not increasing for " << dates[i]);
        }
    }

    // Build vector of times_ (checking dates are sorted)
    times_ = std::vector<Time>(dates.size() + 1);
    times_[0] = 0.0;
    for (Size i = 0; i < dates.size(); i++) {
        times_[i + 1] = timeFromReference(dates[i]);
        QL_REQUIRE(times_[i + 1] > times_[i], "dates must be sorted unique!");
    }

    // default to cubic
    setInterpolation<Cubic>();
}

Real BlackVarianceSurface2::blackVarianceImpl(Time t, Real strike) const {
    std::vector<Real> variance(times_.size());
    variance[0] = 0.0; // times_[0] = 0.0

    for (Size i = 0; i < interpolators_.size(); i++) {
        Real k = strike; // TODO interpolate along a forward curve rather than just strike
        Real vol = interpolators_[i](k);
        variance[i + 1] = times_[i + 1] * vol * vol;
    }
    LinearInterpolation linear(times_.begin(), times_.end(), variance.begin());
    linear.enableExtrapolation();
    return linear(t);
}

} // namespace QuantExt

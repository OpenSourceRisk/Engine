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
#include <qle/termstructures/blackvariancesurfacesparse.hpp>

using namespace std;

namespace QuantExt {

BlackVarianceSurfaceSparse::BlackVarianceSurfaceSparse(const QuantLib::Date& referenceDate, const Calendar& cal,
                                                       const std::vector<Date>& dates, const std::vector<Real>& strikes,
                                                       const std::vector<Volatility>& volatilities,
                                                       const DayCounter& dayCounter)
    : BlackVarianceTermStructure(referenceDate, cal), dayCounter_(dayCounter) {

    QL_REQUIRE((strikes.size() == dates.size()) && (dates.size() == volatilities.size()),
               "dates, strikes and volatilities vectors not of equal size.");
    
    // first get uniques dates and sort
    set<Date> datesSet(dates.begin(), dates.end());
    dates_ = vector<Date>(datesSet.begin(), datesSet.end());
    vector<bool> dateDone(dates_.size(), false);
    times_ = vector<Time>(dates_.size());
    interpolations_ = vector<Interpolation>(dates_.size());
    variances_ = vector<vector<Real> >(dates_.size());
    strikes_ = vector<vector<Real> >(dates_.size());


    // Populate strike info. (Except interpolation obj members)
    for (Size i = 0; i < strikes.size(); i++) {
        vector<Date>::const_iterator found = find(dates_.begin(), dates_.end(), dates[i]);
        QL_REQUIRE(found != dates_.end(), "Date should already be loaded" << dates[i]);
        Size ii = found - dates_.begin(); // index of expiry
        
        if (!dateDone[ii]) {
            // add expiry data if not found
            QL_REQUIRE(dates[i] >= referenceDate, "Expiry date:" << dates[i] << " before asof date: " << referenceDate);
            times_[ii] = timeFromReference(dates[i]);
            strikes_[ii].push_back(strikes[i]);
            variances_[ii].push_back(volatilities[i] * volatilities[i] * times_[ii] );
            dateDone[ii] = true;
        } else {
            // expiry found => add if strike not found
            Real tmpStrike = strikes[i];
            vector<Real>::iterator fnd = find_if(strikes_[ii].begin(), strikes_[ii].end(),QuantExt::detail::CloseEnoughComparator(tmpStrike));
            if (fnd == strikes_[ii].end()) {
                // add strike/var pairs if strike not found for this expiry
                strikes_[ii].push_back(strikes[i]);
                variances_[ii].push_back(volatilities[i] * volatilities[i] * times_[ii]);
            }
        }
    }
    // set expiries' interpolations.
    for (Size i = 0; i < dates_.size(); i++) {
        // sort strikes within this expiry 
        vector<pair<Real, Real> > tmpPairs(strikes_[i].size());
        vector<Real> sortedStrikes; //(itr->second.expStrikes_.size());
        vector<Real> sortedVars;    // (sortedStrikes)
        for (Size j = 0; j < strikes_[i].size(); j++) {
            tmpPairs[j] = { strikes_[i][j], variances_[i][j] };
        }
        sort(tmpPairs.begin(), tmpPairs.end());
        for (vector<pair<Real,Real> >::iterator it = tmpPairs.begin(); it != tmpPairs.end(); it++) {
            sortedStrikes.push_back(it->first);
            sortedVars.push_back(it->second);
        }
        strikes_[i] = sortedStrikes;
        variances_[i] = sortedVars;

        // set interpolation
        if (strikes_[i].size() == 1) {
            // if only one strike => add different strike with same value for interpolation object.
            strikes_[i].push_back(strikes_[i][0] + strikes_[i][0] * 0.3);
            variances_[i].push_back(variances_[i][0]);
        }
        LinearInterpolation tmpInterpolation(strikes_[i].begin(), strikes_[i].end(), variances_[i].begin());
        interpolations_[i] = tmpInterpolation;
    }

    // Short end
    if (dates_[0] != this->referenceDate()) {
        dates_.insert(dates_.begin(), this->referenceDate());
        times_.insert(times_.begin(), 0.0);
        strikes_.insert(strikes_.begin(), vector<Real>({ 5.0, 100.0 }));
        variances_.insert(variances_.begin(), vector<Real>({ 0.0, 0.0 }));
        interpolations_.insert(interpolations_.begin(),
                               LinearInterpolation(strikes_[0].begin(), strikes_[0].end(), variances_[0].begin()));
    } else {
        // all variances at reference date should be zero
        for (Size i = 0; i < variances_[0].size(); i++) {
            QL_REQUIRE(variances_[0][i] == 0.0, "Variance at reference date not equal to zero");
        }
        // time at reference date should be zero
        QL_REQUIRE(times_[0] == 0.0, "Time at the reference date not equal to zero.");
    }
}

Real BlackVarianceSurfaceSparse::getVarForStrike(Real strike, const vector<Real>& strks, const vector<Real>& vars, const Interpolation& intrp) const {
    Real retVar;
    if (strike > strks.back()) {
        retVar = vars.back();               // flat extrapolate far stirke
    } else if (strike < strks.front()) {
        retVar = vars.front();              // flat extrapolate near stirke
    } else {
        retVar = intrp(strike);             // interpolate between strikes
    }
    return retVar;
}

Real BlackVarianceSurfaceSparse::blackVarianceImpl(Time t, Real strike) const {
    QL_REQUIRE(t >= 0, "Variance requested for date before reference date: " << this->referenceDate());
    Real varReturn;
    std::vector<Date>::const_iterator itr;     // expiry just after requested time
    std::vector<Date>::const_iterator itrPrev; // expiry just before requested time
    bool farEnd = true;

    // find expiries to interpolate between
    Size dt;
    Size dtPrev;
    for (dt = 0;  dt < dates_.size(); dt++) {
        if (t < times_[dt]) {
            farEnd = false;
            break;
        }
    }

    if (!farEnd) {
        dtPrev = dt - 1;
        // interpolate between expiries
        vector<Real> tmpVars(2);
        vector<Time> xAxis = vector<Time>({ times_[dtPrev], times_[dt] });
        tmpVars[1] = getVarForStrike(strike, strikes_[dt], variances_[dt], interpolations_[dt]);
        tmpVars[0] = getVarForStrike(strike, strikes_[dtPrev], variances_[dtPrev], interpolations_[dtPrev]);
        LinearInterpolation tmpInterpolation(xAxis.begin(), xAxis.end(), tmpVars.begin());
        varReturn = tmpInterpolation(t);
    } else {
        // far end of expiries

        varReturn = getVarForStrike(strike, strikes_.back(), variances_.back(),interpolations_.back());
        varReturn = varReturn * t / times_.back(); // scale
    }

    return varReturn;
}

} // namespace QuantExt
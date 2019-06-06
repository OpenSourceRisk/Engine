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

#include <qle/interpolators/optioninterpolator2d.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <set>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

OptionInterpolator2d::OptionInterpolator2d(const Date& referenceDate, const DayCounter& dayCounter,
    const vector<Date>& dates, const vector<Real>& strikes, const vector<Real>& values) :
    referenceDate_(referenceDate), dayCounter_(dayCounter), initialised_(false) {
        
    initialise(dates, strikes, values);
};

void OptionInterpolator2d::initialise(const std::vector<Date>& dates, const std::vector<Real>& strikes,
    const std::vector<Real>& values) {
    QL_REQUIRE((strikes.size() == dates.size()) && (dates.size() == values.size()),
        "dates, strikes and values vectors not of equal size.");

    // first get uniques dates and sort
    set<Date> datesSet(dates.begin(), dates.end());
    expiries_ = vector<Date>(datesSet.begin(), datesSet.end());
    vector<bool> dateDone(expiries_.size(), false);
    times_ = vector<Time>(expiries_.size());
    interpolations_ = vector<Interpolation>(expiries_.size());
    values_ = vector<vector<Real> >(expiries_.size());
    strikes_ = vector<vector<Real> >(expiries_.size());

    // Populate expiry info. (Except interpolation obj members)
    for (Size i = 0; i < dates.size(); i++) {
        vector<Date>::iterator found = find(expiries_.begin(), expiries_.end(), dates[i]);
        QL_REQUIRE(found != expiries_.end(), "Date should already be loaded" << dates[i]);
        ptrdiff_t ii;
        ii = distance(expiries_.begin(), found); // index of expiry

        if (!dateDone[ii]) {
            // add expiry data if not found
            QL_REQUIRE(dates[i] >= referenceDate_,
                "Expiry date:" << dates[i] << " before asof date: " << referenceDate_);
            times_[ii] = dayCounter_.yearFraction(referenceDate_, dates[i]);
            strikes_[ii].push_back(strikes[i]);
            values_[ii].push_back(values[i]);
            dateDone[ii] = true;
        } else {
            // expiry found => add if strike not found
            Real tmpStrike = strikes[i];
            vector<Real>::iterator fnd =
                find_if(strikes_[ii].begin(), strikes_[ii].end(), QuantExt::detail::CloseEnoughComparator(tmpStrike));
            if (fnd == strikes_[ii].end()) {
                // add strike/var pairs if strike not found for this expiry
                strikes_[ii].push_back(strikes[i]);
                values_[ii].push_back(values[i]);
            }
        }
    }

    for (Size i = 0; i < expiries_.size(); i++)
        QL_REQUIRE(strikes_[i].size() == values_[i].size(), "different number of variances and strikes for date: " << expiries_[i]);

    // set expiries' interpolations.
    for (Size i = 0; i < expiries_.size(); i++) {
        // sort strikes within this expiry
        vector<pair<Real, Real> > tmpPairs(strikes_[i].size());
        vector<Real> sortedStrikes;
        vector<Real> sortedVals;
        for (Size j = 0; j < strikes_[i].size(); j++) {
            tmpPairs[j] = pair<Real, Real>(strikes_[i][j], values_[i][j]);
        }
        sort(tmpPairs.begin(), tmpPairs.end());         // sorts according to first. (strikes)
        for (vector<pair<Real, Real> >::iterator it = tmpPairs.begin(); it != tmpPairs.end(); it++) {
            sortedStrikes.push_back(it->first);
            sortedVals.push_back(it->second);
        }
        strikes_[i] = sortedStrikes;
        values_[i] = sortedVals;

        // set interpolation
        if (strikes_[i].size() == 1) {
            // if only one strike => add different strike with same value for interpolation object.
            strikes_[i].push_back(strikes_[i][0] + strikes_[i][0] * 2);
            values_[i].push_back(values_[i][0]);
        }
        LinearInterpolation tmpInterpolation(strikes_[i].begin(), strikes_[i].end(), values_[i].begin());
        interpolations_[i] = tmpInterpolation;
    }

    initialised_ = true;
}

Real OptionInterpolator2d::getValueForStrike(Real strike, const vector<Real>& strks, const vector<Real>& vars,
    const Interpolation& intrp) const {

    Real retVar;
    if (strike > strks.back()) {
        retVar = vars.back(); // flat extrapolate far strike
    }
    else if (strike < strks.front()) {
        retVar = vars.front(); // flat extrapolate near strike
    }
    else {
        retVar = intrp(strike); // interpolate between strikes
    }
    return retVar;
}

Real OptionInterpolator2d::getValue(Time t, Real strike) const {
    QL_REQUIRE(initialised_, "No data provided to OptionInterpolator2d");
    QL_REQUIRE(t >= 0, "Variance requested for date before reference date: " << referenceDate_);
    Real varReturn;
    if (t == 0.0) {
        //requested at reference date
        varReturn = values_[0][0];
    } else{
        // index for strikes/times for interpolation
        Size ind1, ind2;      
        if (t <= times_.front()) {
            // near end of expiries, use first 2 strikes
            ind1 = 0;
            ind2 = 1;
        } else if (t > times_.back()) {
            // far end of expiries, use last 2
            ind1 = times_.size() - 2;
            ind2 = times_.size() - 1;
        } else {
            //requested between existing expiries (interpolate between expiries)
            ind2 = distance(times_.begin(), lower_bound(times_.begin(), times_.end(), t));
            ind1 = (ind2 != 0) ? ind2 - 1 : 0;
        }                
        // interpolate between expiries
        vector<Real> tmpVars(2);
        vector<Time> xAxis;
        xAxis.push_back(times_[ind1]);
        xAxis.push_back(times_[ind2]);

        tmpVars[0] = getValueForStrike(strike, strikes_[ind1], values_[ind1], interpolations_[ind1]);
        tmpVars[1] = getValueForStrike(strike, strikes_[ind2], values_[ind2], interpolations_[ind2]);
        LinearInterpolation tmpInterpolation(xAxis.begin(), xAxis.end(), tmpVars.begin());
        // linear extrapolation of expiries
        tmpInterpolation.enableExtrapolation(true);
        varReturn = tmpInterpolation(t);
    }
    return varReturn;
}

Real OptionInterpolator2d::getValue(Date d, Real strike) const {
    QL_REQUIRE(initialised_, "No data provided to OptionInterpolator2d");
    QL_REQUIRE(d >= referenceDate_, "Variance requested for date before reference date: " << referenceDate_);
    Real valueReturn;
    
    vector<Date>::const_iterator it = find(expiries_.begin(), expiries_.end(), d);
    // if the date matches one of the expiries get the value on that day, otherwise use time interpolation
    if (it != expiries_.end()) {
        ptrdiff_t dis = distance(expiries_.begin(), it);
        valueReturn = getValueForStrike(strike, strikes_[dis], values_[dis], interpolations_[dis]);
    } else {
        Time t = dayCounter_.yearFraction(referenceDate_, d);
        valueReturn = getValue(t, strike);
    }

    return valueReturn;
}

vector<Time> OptionInterpolator2d::times() const {
    QL_REQUIRE(initialised_, "No data provided to OptionInterpolator2d");
    return times_;
}

vector<Date> OptionInterpolator2d::expiries() const {
    QL_REQUIRE(initialised_, "No data provided to OptionInterpolator2d");
    return expiries_;
}

vector<vector<Real> > OptionInterpolator2d::strikes() const {
    QL_REQUIRE(initialised_, "No data provided to OptionInterpolator2d");
    return strikes_;
}

}// namespace QuantExt
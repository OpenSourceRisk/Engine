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

struct BlackVarianceSurfaceSparse::ExpData {

    std::vector<Real> expStrikes_;
    std::vector<Real> expVars_;
    Time expTime_;
    Interpolation expStrikeCurve_;
};

BlackVarianceSurfaceSparse::BlackVarianceSurfaceSparse(const QuantLib::Date& referenceDate, const Calendar& cal,
                                                       const std::vector<Date>& dates, const std::vector<Real>& strikes,
                                                       const std::vector<Volatility> volatilities,
                                                       const DayCounter& dayCounter)
    : BlackVarianceTermStructure(referenceDate, cal), dayCounter_(dayCounter), maxDate_(dates.back()),
      strikes_(strikes), dates_(dates), volatilities_(volatilities) {

    QL_REQUIRE((strikes.size() == dates.size()) && (dates.size() == volatilities.size()),
               "dates, strikes and volatilites vectors not of equal size.");

    // populate expiries set map. (Except interpolation obj members)
    for (Size i = 0; i < strikes_.size(); i++) {

        if (expiries_.find(dates_[i]) == expiries_.end()) {
            // add expiry struct if not found
            ExpData tmpData;
            QL_REQUIRE(dates_[i] > referenceDate, "Expiry date before asof date: " << referenceDate);
            tmpData.expTime_ = timeFromReference(dates_[i]);
            vector<Real> tmpStrikes{ strikes_[i] };
            vector<Real> tmpVars{ tmpData.expTime_ * volatilities_[i] * volatilities_[i] };
            tmpData.expStrikes_ = tmpStrikes;
            tmpData.expVars_ = tmpVars;
            expiries_[dates_[i]] = tmpData;
        } else {
            // expiry found
            Real tmpStrike = strikes_[i];
            auto fnd = find_if(expiries_[dates_[i]].expStrikes_.begin(), expiries_[dates_[i]].expStrikes_.end(),
                               [tmpStrike](Real b) { return close(tmpStrike, b, QL_EPSILON); });
            if (fnd == expiries_[dates_[i]].expStrikes_.end()) {
                expiries_[dates_[i]].expStrikes_.push_back(strikes_[i]);
                expiries_[dates_[i]].expVars_.push_back(expiries_[dates_[i]].expTime_ * volatilities_[i] *
                                                        volatilities_[i]);
            }
        }
    }
    // set expiries' interpolation obj members.
    for (auto itr = expiries_.begin(); itr != expiries_.end(); itr++) {
        // sort (improve)
        vector<pair<Real, Real> > tmpPairs(itr->second.expStrikes_.size());
        vector<Real> sortedStrikes; //(itr->second.expStrikes_.size());
        vector<Real> sortedVars;    // (sortedStrikes)
        for (Size i = 0; i, i < itr->second.expStrikes_.size(); i++) {
            tmpPairs[i] = { itr->second.expStrikes_[i], itr->second.expVars_[i] };
        }
        sort(tmpPairs.begin(), tmpPairs.end());
        for (auto it = tmpPairs.begin(); it != tmpPairs.end(); it++) {
            sortedStrikes.push_back(it->first);
            sortedVars.push_back(it->second);
        }
        itr->second.expStrikes_ = sortedStrikes;
        itr->second.expVars_ = sortedVars;

        // set interpolation
        LinearInterpolation tmpInterpolation(itr->second.expStrikes_.begin(), itr->second.expStrikes_.end(),
                                             itr->second.expVars_.begin());
        itr->second.expStrikeCurve_ = tmpInterpolation;
    }

    // Short end
    if (expiries_.find(this->referenceDate()) == expiries_.end()) {
        // ExpData
        ExpData tmpData;
        tmpData.expStrikes_ = vector<Real>({ 5, 10 });
        tmpData.expVars_ = vector<Real>({ 0, 0 });
        tmpData.expTime_ = Real(0);
        LinearInterpolation tmpInterpolation(tmpData.expStrikes_.begin(), tmpData.expStrikes_.end(),
                                             tmpData.expVars_.begin());
        expiries_[this->referenceDate()].expStrikeCurve_ = tmpInterpolation;
    } else {
        QL_REQUIRE(expiries_[this->referenceDate()].expStrikeCurve_(3) == Real(0),
                   "Variance at asof: " << this->referenceDate() << " should be 0");
        QL_REQUIRE(expiries_[this->referenceDate()].expTime_ == Real(0), "Time to asof should be zero.");
    }
}

Real BlackVarianceSurfaceSparse::getVarForStrike(Real strike, ExpData expiryData) const {
    Real retVar;
    if (strike > expiryData.expStrikes_.back()) {
        retVar = expiryData.expVars_.back();                // flat extrapolate far stirke
    } else if (strike < expiryData.expStrikes_.front()) {
        retVar = expiryData.expStrikes_.front();            // flat extrapolate near stirke
    } else {
        retVar = expiryData.expStrikeCurve_(strike);        // interpolate between strikes
    }
    return retVar;
}

Real BlackVarianceSurfaceSparse::blackVarianceImpl(Time t, Real strike) const {
    QL_REQUIRE(t >= 0, "Variance requested for date before reference date: " << this->referenceDate());
    Real varReturn;
    std::map<Date, ExpData>::const_iterator itr;        // expiry just after requested time
    std::map<Date, ExpData>::const_iterator itrPrev;    // expiry just before requested time
    bool farEnd = true;

    // find expiries to interpolate between
    for (itr = expiries_.begin(); itr != expiries_.end(); itr++) {
        if (t < itr->second.expTime_) {
            farEnd = false;
            break;
        }
    }

    if (!farEnd) {
        // interpolate between expiries
        vector<Real> tmpVars(2);
        vector<Time> xAxis = { itrPrev->second.expTime_, itr->second.expTime_ };
        tmpVars[1] = getVarForStrike(strike, itr->second);
        tmpVars[0] = getVarForStrike(strike, itrPrev->second);
        LinearInterpolation tmpInterpolation(xAxis.begin(), xAxis.end(), tmpVars.begin());
        varReturn = tmpInterpolation(t);
    } else {
        // far end of expiries
        varReturn = getVarForStrike(strike, expiries_.end()->second);
        varReturn = varReturn * t / expiries_.end()->second.expTime_;   // scale
    }

    return varReturn;
}

} // namespace QuantExt
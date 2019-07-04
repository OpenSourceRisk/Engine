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

/*! \file blackvariancesurfacesparse.hpp
 \brief Black volatility surface modelled as variance surface
 */

#ifndef quantext_option_interpolator_2d_hpp
#define quantext_option_interpolator_2d_hpp

#include <ql/math/interpolation.hpp>
#include <ql/patterns/observable.hpp>
#include <ql/time/date.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantExt {

struct CloseEnoughComparator {
    explicit CloseEnoughComparator(const QuantLib::Real v) : v_(v) {}
    bool operator()(const QuantLib::Real w) const { return QuantLib::close_enough(v_, w); }
    QuantLib::Real v_;
};

//! Option surface intepolator
//!  \ingroup interpolators
template <class InterpolatorStrike, class InterpolatorExpiry>
class OptionInterpolator2d {

public:
    //! OptionInterpolator2d Constructor
    OptionInterpolator2d(const QuantLib::Date& referenceDate, 
        const QuantLib::DayCounter& dayCounter, 
        const InterpolatorStrike& interpolatorStrike = InterpolatorStrike(),
        const InterpolatorExpiry& interpolatorExpiry = InterpolatorExpiry()) :
        referenceDate_(referenceDate), dayCounter_(dayCounter), interpolatorStrike_(interpolatorStrike),
        interpolatorExpiry_(interpolatorExpiry), initialised_(false) {};

    OptionInterpolator2d(const QuantLib::Date& referenceDate, const QuantLib::DayCounter& dayCounter, 
        const std::vector<QuantLib::Date>& dates, const std::vector<QuantLib::Real>& strikes, 
        const std::vector<QuantLib::Real>& values,
        const InterpolatorStrike& interpolatorStrike = InterpolatorStrike(),
        const InterpolatorExpiry& interpolatorExpiry = InterpolatorExpiry());

    //! Initialise
    void initialise(const std::vector<QuantLib::Date>& dates, const std::vector<QuantLib::Real>& strikes,
        const std::vector<QuantLib::Real>& values);

    //! \name Getters
    //@{
    const QuantLib::Date& referenceDate() const { return referenceDate_; }
    QuantLib::DayCounter dayCounter() const { return dayCounter_; }
    std::vector<QuantLib::Time> times() const;
    std::vector<QuantLib::Date> expiries() const;
    std::vector<std::vector<QuantLib::Real> > strikes() const;
    QuantLib::Real getValue(QuantLib::Time t, QuantLib::Real strike) const;
    QuantLib::Real getValue(QuantLib::Date d, QuantLib::Real strike) const;
    //@}

protected:
    std::vector<QuantLib::Date> expiries_;                            // expiries
    std::vector<QuantLib::Time> times_;                               // times
    std::vector<QuantLib::Interpolation> interpolations_;             // strike interpolations for each expiry
    std::vector<std::vector<QuantLib::Real> > strikes_;               // strikes for each expiry
    std::vector<std::vector<QuantLib::Real> > values_;

private:
    QuantLib::Date referenceDate_;
    QuantLib::DayCounter dayCounter_;
    QuantLib::Real getValueForStrike(QuantLib::Real strike, const std::vector<QuantLib::Real>& strks, 
        const std::vector<QuantLib::Real>& vars, const QuantLib::Interpolation& intrp) const;
    InterpolatorStrike interpolatorStrike_;
    InterpolatorExpiry interpolatorExpiry_;
    bool initialised_;

};

// template definitions
template <class InterpolatorStrike, class InterpolatorExpiry>
OptionInterpolator2d<InterpolatorStrike, InterpolatorExpiry>::OptionInterpolator2d(
    const QuantLib::Date& referenceDate, const QuantLib::DayCounter& dayCounter,
    const std::vector<QuantLib::Date>& dates, const std::vector<QuantLib::Real>& strikes, 
    const std::vector<QuantLib::Real>& values,
    const InterpolatorStrike& interpolatorStrike,
    const InterpolatorExpiry& interpolatorExpiry) :
    referenceDate_(referenceDate), dayCounter_(dayCounter), interpolatorStrike_(interpolatorStrike),
    interpolatorExpiry_(interpolatorExpiry), initialised_(false) {

    initialise(dates, strikes, values);
};

template <class IS, class IE>
void OptionInterpolator2d<IS, IE>::initialise(const std::vector<QuantLib::Date>& dates, 
    const std::vector<QuantLib::Real>& strikes, const std::vector<QuantLib::Real>& values) {
    using QuantLib::Date;
    using QuantLib::Size;
    using QuantLib::Real;
    using QuantLib::Time;
    using QuantLib::Interpolation;
    using std::vector;
    using std::set;
    using std::pair;
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
        Size ii;
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
                find_if(strikes_[ii].begin(), strikes_[ii].end(), CloseEnoughComparator(tmpStrike));
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
        interpolations_[i] = interpolatorStrike_.interpolate(strikes_[i].begin(), strikes_[i].end(), values_[i].begin());
    }
    initialised_ = true;
}

template <class IS, class IE>
QuantLib::Real OptionInterpolator2d<IS, IE>::getValueForStrike(QuantLib::Real strike,
    const std::vector<QuantLib::Real>& strks, const std::vector<QuantLib::Real>& vars,
    const QuantLib::Interpolation& intrp) const {

    QuantLib::Real retVar;
    if (strike > strks.back()) {
        retVar = vars.back(); // flat extrapolate far strike
    } else if (strike < strks.front()) {
        retVar = vars.front(); // flat extrapolate near strike
    } else {
        retVar = intrp(strike); // interpolate between strikes
    }
    return retVar;
}

template <class IS, class IE>
QuantLib::Real OptionInterpolator2d<IS, IE>::getValue(QuantLib::Time t, QuantLib::Real strike) const {
    using QuantLib::Size;
    using QuantLib::Real;
    using QuantLib::Time;
    using QuantLib::Interpolation;
    using std::vector;
    QL_REQUIRE(initialised_, "No data provided to OptionInterpolator2d");
    QL_REQUIRE(t >= 0, "Variance requested for date before reference date: " << referenceDate_);
    Real varReturn;
    if (t == 0.0) {
        //requested at reference date
        varReturn = values_[0][0];
    } else {
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
        Interpolation interp = interpolatorExpiry_.interpolate(xAxis.begin(), xAxis.end(), tmpVars.begin());
        // linear extrapolation of expiries
        interp.enableExtrapolation(true);
        varReturn = interp(t);
    }
    return varReturn;
}

template <class IS, class IE>
QuantLib::Real OptionInterpolator2d<IS, IE>::getValue(QuantLib::Date d, QuantLib::Real strike) const {
    QL_REQUIRE(initialised_, "No data provided to OptionInterpolator2d");
    QL_REQUIRE(d >= referenceDate_, "Variance requested for date before reference date: " << referenceDate_);
    QuantLib::Real valueReturn;

    std::vector<QuantLib::Date>::const_iterator it = find(expiries_.begin(), expiries_.end(), d);
    // if the date matches one of the expiries get the value on that day, otherwise use time interpolation
    if (it != expiries_.end()) {
        QuantLib::Size dis = distance(expiries_.begin(), it);
        valueReturn = getValueForStrike(strike, strikes_[dis], values_[dis], interpolations_[dis]);
    } else {
        QuantLib::Time t = dayCounter_.yearFraction(referenceDate_, d);
        valueReturn = getValue(t, strike);
    }

    return valueReturn;
}

template <class IS, class IE>
std::vector<QuantLib::Time> OptionInterpolator2d<IS, IE>::times() const {
    QL_REQUIRE(initialised_, "No data provided to OptionInterpolator2d");
    return times_;
}

template <class IS, class IE>
std::vector<QuantLib::Date> OptionInterpolator2d<IS, IE>::expiries() const {
    QL_REQUIRE(initialised_, "No data provided to OptionInterpolator2d");
    return expiries_;
}

template <class IS, class IE>
std::vector<std::vector<QuantLib::Real> > OptionInterpolator2d<IS, IE>::strikes() const {
    QL_REQUIRE(initialised_, "No data provided to OptionInterpolator2d");
    return strikes_;
}

} //namespace QuantExt

#endif

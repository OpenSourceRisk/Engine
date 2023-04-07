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
 \brief Black volatility surface modeled as variance surface
 */

#ifndef quantext_option_interpolator_2d_hpp
#define quantext_option_interpolator_2d_hpp

#include <ql/math/interpolation.hpp>
#include <ql/patterns/observable.hpp>
#include <ql/time/date.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

#include <qle/math/constantinterpolation.hpp>

namespace QuantExt {

struct CloseEnoughComparator {
    explicit CloseEnoughComparator(const QuantLib::Real v) : v_(v) {}
    bool operator()(const QuantLib::Real w) const { return QuantLib::close_enough(v_, w); }
    QuantLib::Real v_;
};

//! Option surface interpolator base
class OptionInterpolatorBase {

public:
    // Destructor
    virtual ~OptionInterpolatorBase() {}
    // default Constructor
    explicit OptionInterpolatorBase(const QuantLib::Date& referenceDate) : referenceDate_(referenceDate){};

    //! virtual access methods
    virtual QuantLib::Real getValue(QuantLib::Time t, QuantLib::Real strike) const = 0;
    virtual QuantLib::Real getValue(QuantLib::Date d, QuantLib::Real strike) const = 0;

    const QuantLib::Date& referenceDate() const { return referenceDate_; }
    std::vector<QuantLib::Time> times() const { return times_; };
    std::vector<QuantLib::Date> expiries() const { return expiries_; };
    std::vector<std::vector<QuantLib::Real> > strikes() const { return strikes_; };
    std::vector<std::vector<QuantLib::Real> > values() const { return values_; };

protected:
    std::vector<QuantLib::Date> expiries_;              // expiries
    std::vector<QuantLib::Time> times_;                 // times
    std::vector<std::vector<QuantLib::Real> > strikes_; // strikes for each expiry
    std::vector<std::vector<QuantLib::Real> > values_;
    QuantLib::Date referenceDate_;
};

//! Option surface interpolator
//!  \ingroup interpolators
template <class InterpolatorStrike, class InterpolatorExpiry>
class OptionInterpolator2d : public OptionInterpolatorBase {

public:
    //! OptionInterpolator2d default Constructor
    OptionInterpolator2d(const QuantLib::Date& referenceDate, const QuantLib::DayCounter& dayCounter,
                         bool lowerStrikeConstExtrap = true, bool upperStrikeConstExtrap = true,
                         const InterpolatorStrike& interpolatorStrike = InterpolatorStrike(),
                         const InterpolatorExpiry& interpolatorExpiry = InterpolatorExpiry(),
                         const QuantLib::Date& baseDate = QuantLib::Date())
        : OptionInterpolatorBase(referenceDate), dayCounter_(dayCounter),
          lowerStrikeConstExtrap_(lowerStrikeConstExtrap), upperStrikeConstExtrap_(upperStrikeConstExtrap),
          interpolatorStrike_(interpolatorStrike), interpolatorExpiry_(interpolatorExpiry), initialised_(false),
          baseDate_(baseDate == QuantLib::Date() ? referenceDate : baseDate){};
    
    //! OptionInterpolator2d Constructor with dates
    OptionInterpolator2d(const QuantLib::Date& referenceDate, const QuantLib::DayCounter& dayCounter,
                         const std::vector<QuantLib::Date>& dates, const std::vector<QuantLib::Real>& strikes,
                         const std::vector<QuantLib::Real>& values, bool lowerStrikeConstExtrap = true,
                         bool upperStrikeConstExtrap = true,
                         const InterpolatorStrike& interpolatorStrike = InterpolatorStrike(),
                         const InterpolatorExpiry& interpolatorExpiry = InterpolatorExpiry(),
                         const QuantLib::Date& baseDate = QuantLib::Date());

    //! OptionInterpolator2d Constructor with Tenors
    OptionInterpolator2d(const QuantLib::Date& referenceDate, const QuantLib::Calendar& calendar,
        const QuantLib::BusinessDayConvention& bdc, const QuantLib::DayCounter& dayCounter,
        const std::vector<QuantLib::Period>& tenors, const std::vector<QuantLib::Real>& strikes,
        const std::vector<QuantLib::Real>& values, bool lowerStrikeConstExtrap = true,
        bool upperStrikeConstExtrap = true,
        const InterpolatorStrike& interpolatorStrike = InterpolatorStrike(),
        const InterpolatorExpiry& interpolatorExpiry = InterpolatorExpiry(),
        const QuantLib::Date& baseDate = QuantLib::Date());

    /* delete copy and copy assignment operators because of the stored Interpolation objects, which would
       still point to the source object's data after the copy */
    OptionInterpolator2d(const OptionInterpolator2d&) = delete;
    OptionInterpolator2d& operator=(const OptionInterpolator2d&) = delete;

    //! Initialise
    void initialise(const std::vector<QuantLib::Date>& dates, const std::vector<QuantLib::Real>& strikes,
        const std::vector<QuantLib::Real>& values);
    void initialise(const std::vector<QuantLib::Period>& tenors, const std::vector<QuantLib::Real>& strikes,
        const std::vector<QuantLib::Real>& values, const QuantLib::Calendar& calendar,
        const QuantLib::BusinessDayConvention& bdc);

    //! \name Getters
    //@{
    std::vector<QuantLib::Time> times() const;
    std::vector<QuantLib::Date> expiries() const;
    std::vector<std::vector<QuantLib::Real> > strikes() const;
    std::vector<std::vector<QuantLib::Real> > values() const;
    QuantLib::DayCounter dayCounter() const { return dayCounter_; }
    QuantLib::Real getValue(QuantLib::Time t, QuantLib::Real strike) const override;
    QuantLib::Real getValue(QuantLib::Date d, QuantLib::Real strike) const override;
    //@}

protected:
    std::vector<QuantLib::Interpolation> interpolations_; // strike interpolations for each expiry

private:
    QuantLib::DayCounter dayCounter_;
    QuantLib::Real getValueForStrike(QuantLib::Real strike, const std::vector<QuantLib::Real>& strks,
                                     const std::vector<QuantLib::Real>& vars,
                                     const QuantLib::Interpolation& intrp) const;
    bool lowerStrikeConstExtrap_;
    bool upperStrikeConstExtrap_;
    InterpolatorStrike interpolatorStrike_;
    InterpolatorExpiry interpolatorExpiry_;
    bool initialised_;
    QuantLib::Date baseDate_;

};

// template definitions
template <class InterpolatorStrike, class InterpolatorExpiry>
OptionInterpolator2d<InterpolatorStrike, InterpolatorExpiry>::OptionInterpolator2d(
    const QuantLib::Date& referenceDate, const QuantLib::DayCounter& dayCounter,
    const std::vector<QuantLib::Date>& dates, const std::vector<QuantLib::Real>& strikes,
    const std::vector<QuantLib::Real>& values, bool lowerStrikeConstExtrap, bool upperStrikeConstExtrap,
    const InterpolatorStrike& interpolatorStrike,
    const InterpolatorExpiry& interpolatorExpiry, 
    const QuantLib::Date& baseDate)
    : OptionInterpolatorBase(referenceDate), dayCounter_(dayCounter), lowerStrikeConstExtrap_(lowerStrikeConstExtrap),
      upperStrikeConstExtrap_(upperStrikeConstExtrap), interpolatorStrike_(interpolatorStrike),
      interpolatorExpiry_(interpolatorExpiry), initialised_(false),
      baseDate_(baseDate == QuantLib::Date() ? referenceDate : baseDate) {

    initialise(dates, strikes, values);
};

template <class InterpolatorStrike, class InterpolatorExpiry>
OptionInterpolator2d<InterpolatorStrike, InterpolatorExpiry>::OptionInterpolator2d(
    const QuantLib::Date& referenceDate, const QuantLib::Calendar& calendar,
    const QuantLib::BusinessDayConvention& bdc, const QuantLib::DayCounter& dayCounter,
    const std::vector<QuantLib::Period>& tenors, const std::vector<QuantLib::Real>& strikes,
    const std::vector<QuantLib::Real>& values, bool lowerStrikeConstExtrap, bool upperStrikeConstExtrap, const InterpolatorStrike& interpolatorStrike,
    const InterpolatorExpiry& interpolatorExpiry, const QuantLib::Date& baseDate) 
    : OptionInterpolatorBase(referenceDate), dayCounter_(dayCounter), lowerStrikeConstExtrap_(lowerStrikeConstExtrap),
    upperStrikeConstExtrap_(upperStrikeConstExtrap), interpolatorStrike_(interpolatorStrike),
      interpolatorExpiry_(interpolatorExpiry), initialised_(false),
      baseDate_(baseDate == QuantLib::Date() ? referenceDate : baseDate) {

    initialise(tenors, strikes, values, calendar, bdc);
}

template <class IS, class IE>
void OptionInterpolator2d<IS, IE>::initialise(const std::vector<QuantLib::Date>& dates,
                                              const std::vector<QuantLib::Real>& strikes,
                                              const std::vector<QuantLib::Real>& values) {
    using QuantLib::Date;
    using QuantLib::Interpolation;
    using QuantLib::Real;
    using QuantLib::Size;
    using QuantLib::Time;
    using std::pair;
    using std::set;
    using std::vector;
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

    // check on strikes and values size
    for (Size i = 0; i < expiries_.size(); i++)
        QL_REQUIRE(strikes_[i].size() == values_[i].size(),
            "different number of variances and strikes for date: " << expiries_[i]);

    // set expiries' interpolations.
    for (Size i = 0; i < expiries_.size(); i++) {
        // sort strikes within this expiry
        vector<pair<Real, Real> > tmpPairs(strikes_[i].size());
        vector<Real> sortedStrikes;
        vector<Real> sortedVals;
        for (Size j = 0; j < strikes_[i].size(); j++) {
            tmpPairs[j] = pair<Real, Real>(strikes_[i][j], values_[i][j]);
        }
        sort(tmpPairs.begin(), tmpPairs.end()); // sorts according to first. (strikes)
        for (vector<pair<Real, Real> >::iterator it = tmpPairs.begin(); it != tmpPairs.end(); it++) {
            sortedStrikes.push_back(it->first);
            sortedVals.push_back(it->second);
        }
        strikes_[i] = sortedStrikes;
        values_[i] = sortedVals;

        // set interpolation
        if (strikes_[i].size() == 1) {
            interpolations_[i] = Constant().interpolate(values_[i].front());
        } else {
            interpolations_[i] =
                interpolatorStrike_.interpolate(strikes_[i].begin(), strikes_[i].end(), values_[i].begin());
        }
        interpolations_[i].enableExtrapolation();
    }
    initialised_ = true;
}

template <class IS, class IE>
void OptionInterpolator2d<IS, IE>::initialise(const std::vector<QuantLib::Period>& tenors,
                                              const std::vector<QuantLib::Real>& strikes,
                                              const std::vector<QuantLib::Real>& values, 
                                              const QuantLib::Calendar& calendar,
                                              const QuantLib::BusinessDayConvention& bdc) {

    std::vector<QuantLib::Date> dates;
    for (auto t : tenors)
        dates.push_back(calendar.advance(referenceDate(), t, bdc));

    initialise(dates, strikes, values);
}

template <class IS, class IE>
QuantLib::Real OptionInterpolator2d<IS, IE>::getValueForStrike(QuantLib::Real strike,
                                                               const std::vector<QuantLib::Real>& strks,
                                                               const std::vector<QuantLib::Real>& vars,
                                                               const QuantLib::Interpolation& intrp) const {
    QL_REQUIRE(!strks.empty(), "OptionInterpolator2d: no strikes given");
    QL_REQUIRE(strks.size() == vars.size(), "OptionInterpolator2d: strikes size ("
                                                << strks.size() << ") does not match vars size (" << vars.size()
                                                << ")");
    QuantLib::Real retVar;
    if (strike > strks.back() && upperStrikeConstExtrap_) {
        retVar = vars.back(); // force flat extrapolate far strike if requested
    } else if (strike < strks.front() && lowerStrikeConstExtrap_) {
        retVar = vars.front(); // force flat extrapolate near strike if requested
    } else {
        retVar = intrp(strike); // interpolate between strikes or extrap with interpolator default
    }
    return retVar;
}

template <class IS, class IE>
QuantLib::Real OptionInterpolator2d<IS, IE>::getValue(QuantLib::Time t, QuantLib::Real strike) const {
    using QuantLib::Interpolation;
    using QuantLib::Real;
    using QuantLib::Size;
    using QuantLib::Time;
    using std::vector;
    Time baseTime = dayCounter_.yearFraction(referenceDate_, baseDate_);
    QL_REQUIRE(initialised_, "No data provided to OptionInterpolator2d");
    QL_REQUIRE(t >= baseTime, "Variance requested for date before base date: " << baseDate_);
    if (QuantLib::close_enough(t,baseTime)) {
        // requested at reference date
        QL_REQUIRE(!values_.empty(), "OptionInterpolator2d: no expiries given");
        QL_REQUIRE(!values_.front().empty(), "OptionInterpolator2d: no value for first expiry given");
        return values_[0][0];
    } else {
        QL_REQUIRE(!expiries_.empty(), "OptionInterpolator2d: no expiry given");
        if (expiries_.size() == 1) {
            return getValueForStrike(strike, strikes_[0], values_[0], interpolations_[0]);
        }
        // ind1 and ind2 two expiries on either side of requested time.
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
            // requested between existing expiries (interpolate between expiries)
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
        // linear extrapolation of expiries in case t > time_.back() above.
        interp.enableExtrapolation(true);
        return interp(t);
    }
}

template <class IS, class IE>
QuantLib::Real OptionInterpolator2d<IS, IE>::getValue(QuantLib::Date d, QuantLib::Real strike) const {
    QL_REQUIRE(initialised_, "No data provided to OptionInterpolator2d");
    QL_REQUIRE(d >= baseDate_, "Variance requested for date before reference date: " << baseDate_);
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

template <class IS, class IE> std::vector<QuantLib::Time> OptionInterpolator2d<IS, IE>::times() const {
    QL_REQUIRE(initialised_, "No data provided to OptionInterpolator2d");
    return OptionInterpolatorBase::times();
}

template <class IS, class IE> std::vector<QuantLib::Date> OptionInterpolator2d<IS, IE>::expiries() const {
    QL_REQUIRE(initialised_, "No data provided to OptionInterpolator2d");
    return OptionInterpolatorBase::expiries();
}

template <class IS, class IE> std::vector<std::vector<QuantLib::Real> > OptionInterpolator2d<IS, IE>::strikes() const {
    QL_REQUIRE(initialised_, "No data provided to OptionInterpolator2d");
    return OptionInterpolatorBase::strikes();
}

template <class IS, class IE> std::vector<std::vector<QuantLib::Real> > OptionInterpolator2d<IS, IE>::values() const {
    QL_REQUIRE(initialised_, "No data provided to OptionInterpolator2d");
    return OptionInterpolatorBase::values();
}

} // namespace QuantExt

#endif

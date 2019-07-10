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

/*! \file qle/termstructures/piecewisezerospreadedtermstructure.hpp
    \brief Piecewise-zero-spreaded term structure as in QuantLib,
           but also accepts times instead of dates
*/

#pragma once

#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/yield/zeroyieldstructure.hpp>
#include <vector>

namespace QuantExt {

using namespace QuantLib;

//! Yield curve with an added vector of spreads on the zero-yield rate
/*! The zero-yield spread at any given date is interpolated
    between the input data.

    \note This term structure will remain linked to the original
          structure, i.e., any changes in the latter will be
          reflected in this structure as well.

    \ingroup yieldtermstructures
*/

template <class Interpolator> class InterpolatedPiecewiseZeroSpreadedTermStructure : public ZeroYieldStructure {
public:
    InterpolatedPiecewiseZeroSpreadedTermStructure(const Handle<YieldTermStructure>&,
                                                   const std::vector<Handle<Quote> >& spreads,
                                                   const std::vector<Date>& dates, Compounding comp = Continuous,
                                                   Frequency freq = NoFrequency, const DayCounter& dc = DayCounter(),
                                                   const Interpolator& factory = Interpolator());
    InterpolatedPiecewiseZeroSpreadedTermStructure(const Handle<YieldTermStructure>&,
                                                   const std::vector<Handle<Quote> >& spreads,
                                                   const std::vector<Time>& times, Compounding comp = Continuous,
                                                   Frequency freq = NoFrequency, const DayCounter& dc = DayCounter(),
                                                   const Interpolator& factory = Interpolator());
    //! \name YieldTermStructure interface
    //@{
    DayCounter dayCounter() const;
    Natural settlementDays() const;
    Calendar calendar() const;
    const Date& referenceDate() const;
    Date maxDate() const;
    //@}
protected:
    //! returns the spreaded zero yield rate
    Rate zeroYieldImpl(Time) const;
    void update();

private:
    void updateInterpolation();
    Real calcSpread(Time t) const;
    Handle<YieldTermStructure> originalCurve_;
    std::vector<Handle<Quote> > spreads_;
    std::vector<Date> dates_;
    std::vector<Time> times_;
    std::vector<Spread> spreadValues_;
    Compounding comp_;
    Frequency freq_;
    DayCounter dc_;
    Interpolator factory_;
    Interpolation interpolator_;
};

//! Piecewise zero-spreaded yield curve based on linear interpolation of zero rates
/*! \ingroup yieldtermstructures */

typedef InterpolatedPiecewiseZeroSpreadedTermStructure<Linear> PiecewiseZeroSpreadedTermStructure;

// inline definitions

#ifndef __DOXYGEN__

template <class T>
inline InterpolatedPiecewiseZeroSpreadedTermStructure<T>::InterpolatedPiecewiseZeroSpreadedTermStructure(
    const Handle<YieldTermStructure>& h, const std::vector<Handle<Quote> >& spreads, const std::vector<Date>& dates,
    Compounding comp, Frequency freq, const DayCounter& dc, const T& factory)
    : originalCurve_(h), spreads_(spreads), dates_(dates), times_(dates.size()), spreadValues_(dates.size()),
      comp_(comp), freq_(freq), dc_(dc), factory_(factory) {
    QL_REQUIRE(!spreads_.empty(), "no spreads given");
    QL_REQUIRE(spreads_.size() == dates_.size(), "spread and date vector have different sizes");
    registerWith(originalCurve_);
    for (Size i = 0; i < spreads_.size(); i++)
        registerWith(spreads_[i]);
    if (!originalCurve_.empty())
        updateInterpolation();
}

template <class T>
inline InterpolatedPiecewiseZeroSpreadedTermStructure<T>::InterpolatedPiecewiseZeroSpreadedTermStructure(
    const Handle<YieldTermStructure>& h, const std::vector<Handle<Quote> >& spreads, const std::vector<Time>& times,
    Compounding comp, Frequency freq, const DayCounter& dc, const T& factory)
    : originalCurve_(h), spreads_(spreads), times_(times), spreadValues_(times.size()), comp_(comp), freq_(freq),
      dc_(dc), factory_(factory) {
    QL_REQUIRE(!spreads_.empty(), "no spreads given");
    QL_REQUIRE(spreads_.size() == dates_.size(), "spread and date vector have different sizes");
    registerWith(originalCurve_);
    for (Size i = 0; i < spreads_.size(); i++)
        registerWith(spreads_[i]);
    if (!originalCurve_.empty())
        updateInterpolation();
}

#endif

template <class T> inline DayCounter InterpolatedPiecewiseZeroSpreadedTermStructure<T>::dayCounter() const {
    return originalCurve_->dayCounter();
}

template <class T> inline Calendar InterpolatedPiecewiseZeroSpreadedTermStructure<T>::calendar() const {
    return originalCurve_->calendar();
}

template <class T> inline Natural InterpolatedPiecewiseZeroSpreadedTermStructure<T>::settlementDays() const {
    return originalCurve_->settlementDays();
}

template <class T> inline const Date& InterpolatedPiecewiseZeroSpreadedTermStructure<T>::referenceDate() const {
    return originalCurve_->referenceDate();
}

template <class T> inline Date InterpolatedPiecewiseZeroSpreadedTermStructure<T>::maxDate() const {
    return std::min(originalCurve_->maxDate(), dates_.back());
}

template <class T> inline Rate InterpolatedPiecewiseZeroSpreadedTermStructure<T>::zeroYieldImpl(Time t) const {
    Spread spread = calcSpread(t);
    InterestRate zeroRate = originalCurve_->zeroRate(t, comp_, freq_, true);
    InterestRate spreadedRate(zeroRate + spread, zeroRate.dayCounter(), zeroRate.compounding(), zeroRate.frequency());
    return spreadedRate.equivalentRate(Continuous, NoFrequency, t);
}

template <class T> inline Spread InterpolatedPiecewiseZeroSpreadedTermStructure<T>::calcSpread(Time t) const {
    if (t <= times_.front()) {
        return spreads_.front()->value();
    } else if (t >= times_.back()) {
        return spreads_.back()->value();
    } else {
        return interpolator_(t, true);
    }
}

template <class T> inline void InterpolatedPiecewiseZeroSpreadedTermStructure<T>::update() {
    if (!originalCurve_.empty()) {
        updateInterpolation();
        ZeroYieldStructure::update();
    } else {
        /* The implementation inherited from YieldTermStructure
           asks for our reference date, which we don't have since
           the original curve is still not set. Therefore, we skip
           over that and just call the base-class behavior. */
        TermStructure::update();
    }
}

template <class T> inline void InterpolatedPiecewiseZeroSpreadedTermStructure<T>::updateInterpolation() {
    for (Size i = 0; i < dates_.size(); i++) {
        if (!dates_.empty())
            times_[i] = timeFromReference(dates_[i]);
        spreadValues_[i] = spreads_[i]->value();
    }
    interpolator_ = factory_.interpolate(times_.begin(), times_.end(), spreadValues_.begin());
}

} // namespace QuantExt

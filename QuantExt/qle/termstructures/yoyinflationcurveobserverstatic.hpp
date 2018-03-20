/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/yoyinflationcurveobserverstatic.hpp
    \brief Observable inflation term structure with fixed refernece date based on the
           interpolation of yoy rate quotes.
    \ingroup termstructures
*/

#ifndef quantext_yoy_inflation_curve_observer_static_hpp
#define quantext_yoy_inflation_curve_observer_static_hpp

#include <ql/math/comparison.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Inflation term structure based on the interpolation of zero rates.
/*! \ingroup inflationtermstructures */
template <class Interpolator>
class YoYInflationCurveObserverStatic : public YoYInflationTermStructure,
                                        protected InterpolatedCurve<Interpolator>,
                                        public LazyObject {
public:
    YoYInflationCurveObserverStatic(
        const Date& referenceDate, const Calendar& calendar, const DayCounter& dayCounter, const Period& lag,
        Frequency frequency, bool indexIsInterpolated, const Handle<YieldTermStructure>& yTS,
        const std::vector<Date>& dates, const std::vector<Handle<Quote> >& rates,
        const boost::shared_ptr<Seasonality>& seasonality = boost::shared_ptr<Seasonality>(),
        const Interpolator& interpolator = Interpolator());

    //! \name InflationTermStructure interface
    //@{
    Date baseDate() const;
    Date maxDate() const;
    //@}

    //! \name Inspectors
    //@{
    const std::vector<Date>& dates() const;
    const std::vector<Time>& times() const;
    const std::vector<Real>& data() const;
    const std::vector<Rate>& rates() const;
    std::vector<std::pair<Date, Rate> > nodes() const;
    const std::vector<Handle<Quote> >& quotes() const { return quotes_; };
    //@}

    //! \name Observer interface
    //@{
    void update();
    //@}

private:
    //! \name LazyObject interface
    //@{
    void performCalculations() const;
    //@}

protected:
    //! \name YoYInflationTermStructure Interface
    //@{
    Rate yoyRateImpl(Time t) const;
    //@}
    mutable std::vector<Date> dates_;
    std::vector<Handle<Quote> > quotes_;
};

// template definitions

template <class Interpolator>
YoYInflationCurveObserverStatic<Interpolator>::YoYInflationCurveObserverStatic(
    const Date& referenceDate, const Calendar& calendar, const DayCounter& dayCounter, const Period& lag,
    Frequency frequency, bool indexIsInterpolated, const Handle<YieldTermStructure>& yTS,
    const std::vector<Date>& dates, const std::vector<Handle<Quote> >& rates,
    const boost::shared_ptr<Seasonality>& seasonality, const Interpolator& interpolator)
    : YoYInflationTermStructure(referenceDate, calendar, dayCounter, rates[0]->value(), lag, frequency,
                                indexIsInterpolated, yTS, seasonality),
      InterpolatedCurve<Interpolator>(std::vector<Time>(), std::vector<Real>(), interpolator), dates_(dates),
      quotes_(rates) {

    QL_REQUIRE(dates_.size() > 1, "too few dates: " << dates_.size());

    /**
    // check that the data starts from the beginning,
    // i.e. referenceDate - lag, at least must be in the relevant
    // period
    std::pair<Date,Date> lim =
    inflationPeriod(yTS->referenceDate() - this->observationLag(), frequency);
    QL_REQUIRE(lim.first <= dates_[0] && dates_[0] <= lim.second,
    "first data date is not in base period, date: " << dates_[0]
    << " not within [" << lim.first << "," << lim.second << "]");
    **/

    // by convention, if the index is not interpolated we pull all the dates
    // back to the start of their inflationPeriods
    // otherwise the time calculations will be inconsistent
    if (!indexIsInterpolated_) {
        for (Size i = 0; i < dates_.size(); i++) {
            dates_[i] = inflationPeriod(dates_[i], frequency).first;
        }
    }

    QL_REQUIRE(this->quotes_.size() == dates_.size(),
               "quotes/dates count mismatch: " << this->quotes_.size() << " vs " << dates_.size());

    // initalise data vector, values are copied from quotes in performCalculations()
    this->data_.resize(dates_.size());
    for (Size i = 0; i < dates_.size(); i++)
        this->data_[0] = 0.0;

    this->times_.resize(dates_.size());
    this->times_[0] = timeFromReference(dates_[0]);
    for (Size i = 1; i < dates_.size(); i++) {
        QL_REQUIRE(dates_[i] > dates_[i - 1], "dates not sorted");

        // but must be greater than -1
        QL_REQUIRE(this->data_[i] > -1.0, "zero inflation data < -100 %");

        // this can be negative
        this->times_[i] = timeFromReference(dates_[i]);
        QL_REQUIRE(!close(this->times_[i], this->times_[i - 1]), "two dates correspond to the same time "
                                                                 "under this curve's day count convention");
    }

    this->interpolation_ =
        this->interpolator_.interpolate(this->times_.begin(), this->times_.end(), this->data_.begin());
    this->interpolation_.update();

    // register with each of the quotes
    for (Size i = 0; i < quotes_.size(); i++)
        registerWith(quotes_[i]);
}

template <class T> Date YoYInflationCurveObserverStatic<T>::baseDate() const {
    // if indexIsInterpolated we fixed the dates in the constructor
    calculate();
    return dates_.front();
}

template <class T> Date YoYInflationCurveObserverStatic<T>::maxDate() const {
    Date d;
    if (indexIsInterpolated()) {
        d = dates_.back();
    } else {
        d = inflationPeriod(dates_.back(), frequency()).second;
    }
    return d;
}

template <class T> inline Rate YoYInflationCurveObserverStatic<T>::yoyRateImpl(Time t) const {
    calculate();
    return this->interpolation_(t, true);
}

template <class T> inline const std::vector<Time>& YoYInflationCurveObserverStatic<T>::times() const {
    return this->times_;
}

template <class T> inline const std::vector<Date>& YoYInflationCurveObserverStatic<T>::dates() const { return dates_; }

template <class T> inline const std::vector<Rate>& YoYInflationCurveObserverStatic<T>::rates() const {
    calculate();
    return this->data_;
}

template <class T> inline const std::vector<Real>& YoYInflationCurveObserverStatic<T>::data() const {
    calculate();
    return this->data_;
}

template <class T> inline std::vector<std::pair<Date, Rate> > YoYInflationCurveObserverStatic<T>::nodes() const {
    calculate();
    std::vector<std::pair<Date, Rate> > results(dates_.size());
    for (Size i = 0; i < dates_.size(); ++i)
        results[i] = std::make_pair(dates_[i], this->data_[i]);
    return results;
}

template <class T> inline void YoYInflationCurveObserverStatic<T>::update() {
    LazyObject::update();
    YoYInflationTermStructure::update();
}

template <class T> inline void YoYInflationCurveObserverStatic<T>::performCalculations() const {
    for (Size i = 0; i < dates_.size(); ++i)
        this->data_[i] = quotes_[i]->value();
    this->interpolation_ =
        this->interpolator_.interpolate(this->times_.begin(), this->times_.end(), this->data_.begin());
    this->interpolation_.update();
}
} // namespace QuantExt

#endif

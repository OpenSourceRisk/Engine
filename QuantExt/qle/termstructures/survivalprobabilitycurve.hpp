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

/*! \file qle/termstructures/survivalprobabilitycurve.hpp
    \brief interpolated survival probability term structure
    \ingroup termstructures
*/

#ifndef quantext_survival_probability_curve_hpp
#define quantext_survival_probability_curve_hpp

#include <boost/make_shared.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/credit/survivalprobabilitystructure.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>

using namespace QuantLib;

namespace QuantExt {

//! DefaultProbabilityTermStructure based on interpolation of survival probability quotes
/*! \ingroup termstructures */
template <class Interpolator>
class SurvivalProbabilityCurve : public SurvivalProbabilityStructure,
                                 protected InterpolatedCurve<Interpolator>,
                                 public LazyObject {
public:
    SurvivalProbabilityCurve(const std::vector<Date>& dates, const std::vector<Handle<Quote> >& quotes,
                             const DayCounter& dayCounter, const Calendar& calendar = Calendar(),
                             const std::vector<Handle<Quote> >& jumps = std::vector<Handle<Quote> >(),
                             const std::vector<Date>& jumpDates = std::vector<Date>(),
                             const Interpolator& interpolator = Interpolator());
    //! \name TermStructure interface
    //@{
    Date maxDate() const;
    //@}
    //! \name other inspectors
    //@{
    const std::vector<Time>& times() const;
    const std::vector<Date>& dates() const;
    const std::vector<Real>& data() const;
    const std::vector<Probability>& survivalProbabilities() const;
    const std::vector<Handle<Quote> >& quotes() const;
    std::vector<std::pair<Date, Real> > nodes() const;
    //@}
    //! \name Observer interface
    //@{
    void update();
    //@}
private:
    void initialize();
    //! \name LazyObject interface
    //@{
    void performCalculations() const;
    //@}

    //! \name DefaultProbabilityTermStructure implementation
    //@{
    Probability survivalProbabilityImpl(Time) const;
    Real defaultDensityImpl(Time) const;
    //@}
    mutable std::vector<Date> dates_;
    std::vector<Handle<Quote> > quotes_;
};

// inline definitions

template <class T> inline Date SurvivalProbabilityCurve<T>::maxDate() const { return dates_.back(); }

template <class T> inline const std::vector<Time>& SurvivalProbabilityCurve<T>::times() const { return this->times_; }

template <class T> inline const std::vector<Date>& SurvivalProbabilityCurve<T>::dates() const { return dates_; }

template <class T> inline const std::vector<Real>& SurvivalProbabilityCurve<T>::data() const { return this->data_; }

template <class T> inline const std::vector<Probability>& SurvivalProbabilityCurve<T>::survivalProbabilities() const {
    return this->data_;
}

template <class T> inline std::vector<std::pair<Date, Real> > SurvivalProbabilityCurve<T>::nodes() const {
    std::vector<std::pair<Date, Real> > results(dates_.size());
    for (Size i = 0; i < dates_.size(); ++i)
        results[i] = std::make_pair(dates_[i], this->data_[i]);
    return results;
}

// template definitions

template <class T> Probability SurvivalProbabilityCurve<T>::survivalProbabilityImpl(Time t) const {
    calculate();
    if (t <= this->times_.back())
        return this->interpolation_(t, true);

    // flat hazard rate extrapolation
    Time tMax = this->times_.back();
    Probability sMax = this->data_.back();
    Rate hazardMax = -this->interpolation_.derivative(tMax) / sMax;
    return sMax * std::exp(-hazardMax * (t - tMax));
}

template <class T> Real SurvivalProbabilityCurve<T>::defaultDensityImpl(Time t) const {
    calculate();
    if (t <= this->times_.back())
        return -this->interpolation_.derivative(t, true);

    // flat hazard rate extrapolation
    Time tMax = this->times_.back();
    Probability sMax = this->data_.back();
    Rate hazardMax = -this->interpolation_.derivative(tMax) / sMax;
    return sMax * hazardMax * std::exp(-hazardMax * (t - tMax));
}

template <class T>
SurvivalProbabilityCurve<T>::SurvivalProbabilityCurve(const std::vector<Date>& dates,
                                                      const std::vector<Handle<Quote> >& quotes,
                                                      const DayCounter& dayCounter, const Calendar& calendar,
                                                      const std::vector<Handle<Quote> >& jumps,
                                                      const std::vector<Date>& jumpDates, const T& interpolator)
    : SurvivalProbabilityStructure(dates.front(), calendar, dayCounter, jumps, jumpDates),
      InterpolatedCurve<T>(std::vector<Time>(), std::vector<Real>(), interpolator), dates_(dates), quotes_(quotes) {
    QL_REQUIRE(dates_.size() >= T::requiredPoints, "not enough input dates given");
    QL_REQUIRE(quotes_.size() == dates_.size(), "dates/data count mismatch");
    for (Size i = 0; i < quotes_.size(); i++)
        registerWith(quotes_[i]);
    initialize();
}

template <class T> inline void SurvivalProbabilityCurve<T>::update() {
    LazyObject::update();
    DefaultProbabilityTermStructure::update();
}

template <class T> inline void SurvivalProbabilityCurve<T>::performCalculations() const {
    for (Size i = 0; i < dates_.size(); ++i)
        this->data_[i] = quotes_[i]->value();

    this->interpolation_ =
        this->interpolator_.interpolate(this->times_.begin(), this->times_.end(), this->data_.begin());
    this->interpolation_.update();
}

template <class T> void SurvivalProbabilityCurve<T>::initialize() {
    QL_REQUIRE(dates_.size() >= T::requiredPoints, "not enough input dates given");
    QL_REQUIRE(quotes_.size() == dates_.size(), "dates/data count mismatch");

    this->times_.resize(dates_.size());
    this->data_.resize(dates_.size());
    this->times_[0] = 0.0;
    for (Size i = 1; i < dates_.size(); ++i) {
        QL_REQUIRE(dates_[i] > dates_[i - 1], "invalid date (" << dates_[i] << ", vs " << dates_[i - 1] << ")");
        this->times_[i] = dayCounter().yearFraction(dates_[0], dates_[i]);
        QL_REQUIRE(!close(this->times_[i], this->times_[i - 1]), "two dates correspond to the same time "
                                                                 "under this curve's day count convention");
        // QL_REQUIRE(this->data_[i] > 0.0, "negative probability");
        // QL_REQUIRE(this->data_[i] <= this->data_[i-1],
        //            "negative hazard rate implied by the survival "
        //            "probability " <<
        //            this->data_[i] << " at " << dates_[i] <<
        //            " (t=" << this->times_[i] << ") after the survival "
        //            "probability " <<
        //            this->data_[i-1] << " at " << dates_[i-1] <<
        //            " (t=" << this->times_[i-1] << ")");
    }
}
} // namespace QuantExt

#endif

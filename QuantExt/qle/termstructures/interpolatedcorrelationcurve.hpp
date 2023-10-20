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

/*! \file interpolatedcorrelationcurve.hpp
    \brief interpolated correlation term structure
*/

#ifndef quantext_interpolated_correlation_curve_hpp
#define quantext_interpolated_correlation_curve_hpp

#include <ql/math/interpolations/backwardflatinterpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <qle/termstructures/correlationtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! CorrelationTermStructure based on interpolation of correlations
/*! \ingroup correlationtermstructures */
template <class Interpolator>
class InterpolatedCorrelationCurve : public CorrelationTermStructure,
                                     protected InterpolatedCurve<Interpolator>,
                                     public LazyObject {
public:
    /*! InterpolatedCorrelationCurve has floating referenceDate (Settings::evaluationDate())
     */
    InterpolatedCorrelationCurve(const std::vector<Time>& times, const std::vector<Handle<Quote> >& correlations,
                                 const DayCounter& dayCounter, const Calendar& calendar,
                                 const Interpolator& interpolator = Interpolator());
    //@}
    //! \name TermStructure interface
    //@{
    Date maxDate() const override { return Date::maxDate(); } // flat extrapolation
    Time maxTime() const override { return QL_MAX_REAL; }
    //@}
    //! \name Observer interface
    //@{
    void update() override;
    //@}
private:
    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}
protected:
    //! \name CorrelationTermStructure implementation
    //@{
    Real correlationImpl(Time t, Real) const override;
    //@}
    std::vector<Handle<Quote> > quotes_;
};

// inline definitions

#ifndef __DOXYGEN__

// template definitions

template <class T> Real InterpolatedCorrelationCurve<T>::correlationImpl(Time t, Real) const {
    calculate();
    if (t <= this->times_[0]) {
        return this->data_[0];
    } else if (t <= this->times_.back()) {
        return this->interpolation_(t, true);
    }
    // flat extrapolation
    return this->data_.back();
}

template <class T>
InterpolatedCorrelationCurve<T>::InterpolatedCorrelationCurve(const std::vector<Time>& times,
                                                              const std::vector<Handle<Quote> >& quotes,
                                                              const DayCounter& dayCounter, const Calendar& calendar,
                                                              const T& interpolator)
    : CorrelationTermStructure(0, calendar, dayCounter), InterpolatedCurve<T>(std::vector<Time>(), std::vector<Real>(),
                                                                              interpolator),
      quotes_(quotes) {

    QL_REQUIRE(times.size() > 1, "too few times: " << times.size());
    this->times_.resize(times.size());
    this->times_[0] = times[0];
    for (Size i = 1; i < times.size(); i++) {
        QL_REQUIRE(times[i] > times[i - 1], "times not sorted");
        this->times_[i] = times[i];
    }

    QL_REQUIRE(this->quotes_.size() == this->times_.size(),
               "quotes/times count mismatch: " << this->quotes_.size() << " vs " << this->times_.size());

    // initalise data vector, values are copied from quotes in performCalculations()
    this->data_.resize(this->times_.size());
    for (Size i = 0; i < this->times_.size(); i++)
        this->data_[0] = 0.0;

    this->interpolation_ =
        this->interpolator_.interpolate(this->times_.begin(), this->times_.end(), this->data_.begin());
    this->interpolation_.update();

    // register with each of the quotes
    for (Size i = 0; i < this->quotes_.size(); i++) {
        QL_REQUIRE(std::fabs(this->quotes_[i]->value()) <= 1.0,
                   "correlation not in range (-1.0,1.0): " << this->data_[i]);

        registerWith(this->quotes_[i]);
    }
}

template <class T> inline void InterpolatedCorrelationCurve<T>::update() {
    LazyObject::update();
    CorrelationTermStructure::update();
}

template <class T> inline void InterpolatedCorrelationCurve<T>::performCalculations() const {

    for (Size i = 0; i < this->times_.size(); ++i)
        this->data_[i] = quotes_[i]->value();
    this->interpolation_ =
        this->interpolator_.interpolate(this->times_.begin(), this->times_.end(), this->data_.begin());
    this->interpolation_.update();
}

#endif

typedef InterpolatedCorrelationCurve<BackwardFlat> BackwardFlatCorrelationCurve;
typedef InterpolatedCorrelationCurve<Linear> PiecewiseLinearCorrelationCurve;
} // namespace QuantExt

#endif

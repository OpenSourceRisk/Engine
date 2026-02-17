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

/*! \file qle/termstructures/zeroinflationcurveobservermoving.hpp
    \brief Observable inflation term structure based on the interpolation of zero rate quotes,
           but with floating reference date
*/

#ifndef quantext_zero_inflation_curve_observer_moving_hpp
#define quantext_zero_inflation_curve_observer_moving_hpp

#include <ql/math/comparison.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Inflation term structure based on the interpolation of zero rates, with floating reference date
/*! \ingroup termstructures */
template <class Interpolator>
class ZeroInflationCurveObserverMoving : public ZeroInflationTermStructure,
                                         protected InterpolatedCurve<Interpolator>,
                                         public LazyObject {
public:
    ZeroInflationCurveObserverMoving(
        Natural settlementDays, const Calendar& calendar, const DayCounter& dayCounter, const int simulationLag,
        const Period& observationLag, Frequency frequency, bool indexIsInterpolated, const std::vector<Period>& tenors,
        const std::vector<Handle<Quote>>& rates,
        const QuantLib::ext::shared_ptr<Seasonality>& seasonality = QuantLib::ext::shared_ptr<Seasonality>(),
        const Interpolator& interpolator = Interpolator());

    //! \name InflationTermStructure interface
    //@{
    Date baseDate() const override;
    Time maxTime() const override;
    Date maxDate() const override;
    //@}

    //! \name Inspectors
    //@{
    const std::vector<Time>& times() const;
    const std::vector<Real>& data() const;
    const std::vector<Rate>& rates() const;
    // std::vector<std::pair<Time, Rate> > nodes() const;
    const std::vector<Handle<Quote>>& quotes() const { return quotes_; };
    //@}

    //! \name Observer interface
    //@{
    void update() override;
    //@}

private:
    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    void updateBaseDate() const;
    //@}

protected:
    //! \name ZeroInflationTermStructure Interface
    //@{
    Rate zeroRateImpl(Time t) const override;
    //@}
    std::vector<Handle<Quote>> quotes_;
    std::vector<Period> tenors_;
    mutable Date baseDate_;
    Period observationLag_;
    int simulationLag_;
};

// template definitions

template <class Interpolator>
ZeroInflationCurveObserverMoving<Interpolator>::ZeroInflationCurveObserverMoving(
    Natural settlementDays, const Calendar& calendar, const DayCounter& dayCounter, const int simulationLag,
    const Period& observationLag, Frequency frequency, bool indexIsInterpolated, const std::vector<Period>& tenors,
    const std::vector<Handle<Quote>>& rates, const QuantLib::ext::shared_ptr<Seasonality>& seasonality,
    const Interpolator& interpolator)
    : ZeroInflationTermStructure(settlementDays, calendar, Date(), observationLag, frequency, dayCounter, seasonality),
      InterpolatedCurve<Interpolator>(std::vector<Time>(), std::vector<Real>(), interpolator), quotes_(rates),
      tenors_(tenors), observationLag_(observationLag), simulationLag_(simulationLag) {

    QL_REQUIRE(tenors.size() > 1, "too few tenors: " << tenors.size());
    //std::cout << "update base date"<<std::endl;
    this->times_.resize(tenors_.size());
    updateBaseDate();
    for (Size i = 0; i < tenors_.size(); i++) {
        this->times_[i] = dayCounter.yearFraction(
            referenceDate(), inflationPeriod(referenceDate() + tenors_[i] - observationLag_, frequency).first);
        QL_REQUIRE(i == 0 || this->times_[i] > this->times_[i - 1],
                   "non increasing times: " << this->times_[i - 1] << " vs " << this->times_[i]);
    }

    QL_REQUIRE(this->quotes_.size() == this->times_.size(),
               "quotes/times count mismatch: " << this->quotes_.size() << " vs " << this->times_.size());

    // initialise data vector, values are copied from quotes in performCalculations()
    this->data_.resize(this->times_.size());
    for (Size i = 0; i < this->times_.size(); i++)
        this->data_[0] = 0.0;

    this->interpolation_ =
        this->interpolator_.interpolate(this->times_.begin(), this->times_.end(), this->data_.begin());
    this->interpolation_.update();

    // register with each of the quotes
    for (Size i = 0; i < this->quotes_.size(); i++)
        registerWith(this->quotes_[i]);
}

template <class T> Date ZeroInflationCurveObserverMoving<T>::baseDate() const {
    // if indexIsInterpolated we fixed the dates in the constructor
    calculate();
    return baseDate_;
}

template <class T> Time ZeroInflationCurveObserverMoving<T>::maxTime() const { return this->times_.back(); }

template <class T> Date ZeroInflationCurveObserverMoving<T>::maxDate() const { return this->maxDate_; }

template <class T> inline Rate ZeroInflationCurveObserverMoving<T>::zeroRateImpl(Time t) const {
    calculate();
    return this->interpolation_(t, true);
}

template <class T> inline const std::vector<Time>& ZeroInflationCurveObserverMoving<T>::times() const {
    return this->times_;
}

template <class T> inline const std::vector<Rate>& ZeroInflationCurveObserverMoving<T>::rates() const {
    calculate();
    return this->data_;
}

template <class T> inline const std::vector<Real>& ZeroInflationCurveObserverMoving<T>::data() const {
    calculate();
    return this->data_;
}

template <class T> inline void ZeroInflationCurveObserverMoving<T>::update() {
    LazyObject::update();
    ZeroInflationTermStructure::update();
}

template <class T> inline void ZeroInflationCurveObserverMoving<T>::performCalculations() const {
    updateBaseDate();
    for (Size i = 0; i < this->times_.size(); ++i)
        this->data_[i] = quotes_[i]->value();
    this->interpolation_ =
        this->interpolator_.interpolate(this->times_.begin(), this->times_.end(), this->data_.begin());
    this->interpolation_.update();
}

template <class T> inline void ZeroInflationCurveObserverMoving<T>::updateBaseDate() const {
    Date d = Settings::instance().evaluationDate();
    baseDate_ = inflationPeriod(d - simulationLag_, frequency()).first;
    for (Size i = 0; i < tenors_.size(); i++) {
        this->times_[i] = dayCounter().yearFraction(
            referenceDate(), inflationPeriod(referenceDate() + tenors_[i] - observationLag_, frequency()).first);
    }
}

} // namespace QuantExt

#endif

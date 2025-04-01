/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#pragma once

#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/iterativebootstrap.hpp>
#include <qle/termstructures/inflation/inflationtraits.hpp>
#include <utility>

namespace QuantExt {

//! Piecewise zero-inflation term structure
template <class Interpolator, template <class> class Bootstrap = QuantLib::IterativeBootstrap, class Traits = CPITraits>
class PiecewiseCPIInflationCurve : public Traits::template curve<Interpolator>::type, public QuantLib::LazyObject {
private:
    typedef typename Traits::template curve<Interpolator>::type base_curve;
    typedef PiecewiseCPIInflationCurve<Interpolator, Bootstrap, Traits> this_curve;

public:
    typedef Traits traits_type;
    typedef Interpolator interpolator_type;

    //! \name Constructors
    //@{
    PiecewiseCPIInflationCurve(const QuantLib::Date& referenceDate, QuantLib::Date baseDate, QuantLib::Rate baseCPI,
                               const QuantLib::Period& lag, QuantLib::Frequency frequency,
                               const QuantLib::DayCounter& dayCounter,
                               std::vector<QuantLib::ext::shared_ptr<typename Traits::helper>> instruments,
                               const QuantLib::ext::shared_ptr<QuantLib::Seasonality>& seasonality = {},
                               QuantLib::Real accuracy = 1.0e-14, const Interpolator& i = Interpolator())
        : base_curve(referenceDate, baseDate, baseCPI, lag, frequency, dayCounter, seasonality, i),
          instruments_(std::move(instruments)), accuracy_(accuracy) {
        bootstrap_.setup(this);
    }

    //! \name Inflation interface
    //@{
    QuantLib::Date baseDate() const override;
    QuantLib::Date maxDate() const override;
    //@
    //! \name Inspectors
    //@{
    const std::vector<QuantLib::Time>& times() const override;
    const std::vector<QuantLib::Date>& dates() const override;
    const std::vector<QuantLib::Real>& data() const override;
    const std::vector<QuantLib::Rate>& rates() const override;
    std::vector<std::pair<QuantLib::Date, QuantLib::Real>> nodes() const override;
    //@}
    //! \name Observer interface
    //@{
    void update() override;
    //@}
protected:
    QuantLib::Rate forwardCPIImpl(QuantLib::Time t) const override;

private:
    // methods
    void performCalculations() const override;
    // data members
    std::vector<QuantLib::ext::shared_ptr<typename Traits::helper>> instruments_;
    QuantLib::Real accuracy_;

    friend class Bootstrap<this_curve>;
    friend class QuantLib::BootstrapError<this_curve>;
    Bootstrap<this_curve> bootstrap_;
};

// inline and template definitions

template <class I, template <class> class B, class T>
inline QuantLib::Date PiecewiseCPIInflationCurve<I, B, T>::baseDate() const {
    if (!this->hasExplicitBaseDate())
        this->calculate();
    return base_curve::baseDate();
}

template <class I, template <class> class B, class T>
inline QuantLib::Date PiecewiseCPIInflationCurve<I, B, T>::maxDate() const {
    this->calculate();
    return base_curve::maxDate();
}

template <class I, template <class> class B, class T>
const std::vector<QuantLib::Time>& PiecewiseCPIInflationCurve<I, B, T>::times() const {
    this->calculate();
    return base_curve::times();
}

template <class I, template <class> class B, class T>
const std::vector<QuantLib::Date>& PiecewiseCPIInflationCurve<I, B, T>::dates() const {
    this->calculate();
    return base_curve::dates();
}

template <class I, template <class> class B, class T>
const std::vector<QuantLib::Real>& PiecewiseCPIInflationCurve<I, B, T>::data() const {
    this->calculate();
    return base_curve::rates();
}

template <class I, template <class> class B, class T>
const std::vector<QuantLib::Real>& PiecewiseCPIInflationCurve<I, B, T>::rates() const {
    this->calculate();
    return base_curve::rates();
}

template <class I, template <class> class B, class T>
std::vector<std::pair<QuantLib::Date, QuantLib::Real>> PiecewiseCPIInflationCurve<I, B, T>::nodes() const {
    this->calculate();
    return base_curve::nodes();
}

template <class I, template <class> class B, class T>
void PiecewiseCPIInflationCurve<I, B, T>::performCalculations() const {
    bootstrap_.calculate();
}

template <class I, template <class> class B, class T> void PiecewiseCPIInflationCurve<I, B, T>::update() {
    base_curve::update();
    LazyObject::update();
}

template <class I, template <class> class B, class T>
QuantLib::Rate PiecewiseCPIInflationCurve<I, B, T>::forwardCPIImpl(QuantLib::Time t) const {
    this->calculate();
    return base_curve::forwardCPIImpl(t);
}
} // namespace QuantExt

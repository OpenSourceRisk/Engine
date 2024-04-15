/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.a
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file qle/termstructures/inflation/piecewisezeroinflationcurve.hpp
    \brief Piecewise interpolated zero inflation term structure
    \ingroup termstructures
*/

#pragma once

#include <ql/indexes/inflationindex.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/iterativebootstrap.hpp>
#include <qle/termstructures/inflation/inflationtraits.hpp>
#include <qle/utilities/inflation.hpp>
#include <utility>

namespace QuantExt {

//! Piecewise zero-inflation term structure
template <class Interpolator, template <class> class Bootstrap = QuantLib::IterativeBootstrap,
          class Traits = QuantExt::ZeroInflationTraits>
class PiecewiseZeroInflationCurve : public QuantLib::InterpolatedZeroInflationCurve<Interpolator>,
                                    public QuantLib::LazyObject,
                                    public Traits::BootstrapFirstDateInitializer {
private:
    typedef QuantLib::InterpolatedZeroInflationCurve<Interpolator> base_curve;
    typedef QuantExt::PiecewiseZeroInflationCurve<Interpolator, Bootstrap, Traits> this_curve;

public:
    typedef Traits traits_type;
    typedef Interpolator interpolator_type;

    //! \name Constructors
    //@{
    PiecewiseZeroInflationCurve(const QuantLib::Date& referenceDate, const QuantLib::Calendar& calendar,
                                const QuantLib::DayCounter& dayCounter, const QuantLib::Period& lag,
                                QuantLib::Frequency frequency, QuantLib::Rate baseZeroRate,
                                std::vector<QuantLib::ext::shared_ptr<typename Traits::helper>> instruments,
                                QuantLib::Real accuracy = 1.0e-12,
                                QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex> index = nullptr,
                                bool useLastAvailableFixingAsBaseDate = false, const Interpolator& i = Interpolator())
        : base_curve(referenceDate, calendar, dayCounter, lag, frequency, baseZeroRate, i),
          instruments_(std::move(instruments)), accuracy_(accuracy), index_(index),
          useLastAvailableFixingAsBaseDate_(useLastAvailableFixingAsBaseDate) {
        bootstrap_.setup(this);
    }
    //@}

    //! \name Inflation interface
    //@{
    QuantLib::Date baseDate() const override;
    QuantLib::Date maxDate() const override;
    void setSeasonality(const QuantLib::ext::shared_ptr<QuantLib::Seasonality>& seasonality =
                            QuantLib::ext::shared_ptr<QuantLib::Seasonality>()) override;
    //@
    //! \name Inspectors
    //@{
    const std::vector<QuantLib::Time>& times() const;
    const std::vector<QuantLib::Date>& dates() const;
    const std::vector<QuantLib::Real>& data() const;
    std::vector<std::pair<QuantLib::Date, QuantLib::Real>> nodes() const;
    //@}
    //! \name Observer interface
    //@{
    void update() override;
    //@}
    //! \name Trait::BootstrapFirstDateInitializer interface
    //@{
    QuantLib::Date initialDate() const override;
    //@}
private:
    // methods
    void performCalculations() const override;
    // data members
    std::vector<QuantLib::ext::shared_ptr<typename Traits::helper>> instruments_;
    QuantLib::Real accuracy_;

    friend class Bootstrap<this_curve>;
    friend class QuantLib::BootstrapError<this_curve>;
    Bootstrap<this_curve> bootstrap_;
    QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex> index_;
    bool useLastAvailableFixingAsBaseDate_;
};

// inline and template definitions

template <class I, template <class> class B, class T>
inline QuantLib::Date PiecewiseZeroInflationCurve<I, B, T>::baseDate() const {
    this->calculate();
    return base_curve::baseDate();
}

template <class I, template <class> class B, class T>
inline QuantLib::Date PiecewiseZeroInflationCurve<I, B, T>::maxDate() const {
    this->calculate();
    return base_curve::maxDate();
}

template <class I, template <class> class B, class T>
const std::vector<QuantLib::Time>& PiecewiseZeroInflationCurve<I, B, T>::times() const {
    calculate();
    return base_curve::times();
}

template <class I, template <class> class B, class T>
const std::vector<QuantLib::Date>& PiecewiseZeroInflationCurve<I, B, T>::dates() const {
    calculate();
    return base_curve::dates();
}

template <class I, template <class> class B, class T>
const std::vector<QuantLib::Real>& PiecewiseZeroInflationCurve<I, B, T>::data() const {
    calculate();
    return base_curve::rates();
}

template <class I, template <class> class B, class T>
std::vector<std::pair<QuantLib::Date, QuantLib::Real>> PiecewiseZeroInflationCurve<I, B, T>::nodes() const {
    calculate();
    return base_curve::nodes();
}

template <class I, template <class> class B, class T>
void PiecewiseZeroInflationCurve<I, B, T>::performCalculations() const {
    bootstrap_.calculate();
}

template <class I, template <class> class B, class T> void PiecewiseZeroInflationCurve<I, B, T>::update() {
    base_curve::update();
    LazyObject::update();
}

template <class I, template <class> class B, class T>
QuantLib::Date PiecewiseZeroInflationCurve<I, B, T>::initialDate() const {
    return ZeroInflation::curveBaseDate(useLastAvailableFixingAsBaseDate_, base_curve::referenceDate(),
                                        base_curve::observationLag(), base_curve::frequency(), index_);
}

template <class I, template <class> class B, class T>
void PiecewiseZeroInflationCurve<I, B, T>::setSeasonality(const QuantLib::ext::shared_ptr<QuantLib::Seasonality>& seasonality) {
    // always reset, whether with null or new pointer
    base_curve::seasonality_ = seasonality;
    if (base_curve::seasonality_ != nullptr) {
        QL_REQUIRE(base_curve::seasonality_->isConsistent(*this), "Seasonality inconsistent with "
                                                                  "inflation term structure");
    }
    update();
}

} // namespace QuantExt

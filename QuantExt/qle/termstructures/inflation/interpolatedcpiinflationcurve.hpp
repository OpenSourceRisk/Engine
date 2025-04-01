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

#include <ql/math/comparison.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <qle/termstructures/inflation/cpicurve.hpp>
#include <utility>

namespace QuantExt {

//! Inflation term structure based on the interpolation of zero rates.
/*! \ingroup inflationtermstructures */
template <class Interpolator>
class InterpolatedCPIInflationCurve : public CPICurve, protected QuantLib::InterpolatedCurve<Interpolator> {
public:
    InterpolatedCPIInflationCurve(const QuantLib::Date& referenceDate, std::vector<QuantLib::Date> dates,
                                  const std::vector<QuantLib::Rate>& cpis, const QuantLib::Period& lag,
                                  QuantLib::Frequency frequency, const QuantLib::DayCounter& dayCounter,
                                  const QuantLib::ext::shared_ptr<QuantLib::Seasonality>& seasonality = {},
                                  const Interpolator& interpolator = Interpolator());

    //! \name InflationTermStructure interface
    //@{
    QuantLib::Date baseDate() const override;
    QuantLib::Date maxDate() const override;
    //@}

    //! \name Inspectors
    //@{
    virtual const std::vector<QuantLib::Date>& dates() const;
    virtual const std::vector<QuantLib::Time>& times() const;
    virtual const std::vector<QuantLib::Real>& data() const;
    virtual const std::vector<QuantLib::Rate>& rates() const;
    virtual std::vector<std::pair<QuantLib::Date, QuantLib::Rate>> nodes() const;
    //@}

protected:
    //! \name ZeroInflationTermStructure Interface
    //@{
    QuantLib::Rate forwardCPIImpl(QuantLib::Time t) const override;
    //@}
    mutable std::vector<QuantLib::Date> dates_;
    /*! Protected version for use when descendents don't want to
        (or can't) provide the points for interpolation on
        construction.
    */
    InterpolatedCPIInflationCurve(const QuantLib::Date& referenceDate, const QuantLib::Date& baseDate,
                                  QuantLib::Rate baseCPI, const QuantLib::Period& lag, QuantLib::Frequency frequency,
                                  const QuantLib::DayCounter& dayCounter,
                                  const QuantLib::ext::shared_ptr<QuantLib::Seasonality>& seasonality = {},
                                  const Interpolator& interpolator = Interpolator());
};

typedef InterpolatedCPIInflationCurve<QuantLib::Linear> CPIInflationCurve;

// template definitions

template <class Interpolator>
InterpolatedCPIInflationCurve<Interpolator>::InterpolatedCPIInflationCurve(
    const QuantLib::Date& referenceDate, std::vector<QuantLib::Date> dates, const std::vector<QuantLib::Rate>& cpi,
    const QuantLib::Period& lag, QuantLib::Frequency frequency, const QuantLib::DayCounter& dayCounter,
    const QuantLib::ext::shared_ptr<QuantLib::Seasonality>& seasonality, const Interpolator& interpolator)
    : CPICurve(referenceDate, dates.at(0), cpi.at(0), lag, frequency, dayCounter, seasonality),
      QuantLib::InterpolatedCurve<Interpolator>(std::vector<QuantLib::Time>(), cpi, interpolator),
      dates_(std::move(dates)) {

    QL_REQUIRE(dates_.size() > 1, "too few dates: " << dates_.size());

    QL_REQUIRE(this->data_.size() == dates_.size(),
               "indices/dates count mismatch: " << this->data_.size() << " vs " << dates_.size());
    for (QuantLib::Size i = 1; i < dates_.size(); i++) {
        // must be greater than 0
        QL_REQUIRE(this->data_[i] > 0, "cpi inflation data < 0");
    }

    this->setupTimes(dates_, referenceDate, dayCounter);
    this->setupInterpolation();
    this->interpolation_.update();
}

template <class Interpolator>
InterpolatedCPIInflationCurve<Interpolator>::InterpolatedCPIInflationCurve(
    const QuantLib::Date& referenceDate, const QuantLib::Date& baseDate, QuantLib::Rate baseCPI,
    const QuantLib::Period& lag, QuantLib::Frequency frequency, const QuantLib::DayCounter& dayCounter,
    const QuantLib::ext::shared_ptr<QuantLib::Seasonality>& seasonality, const Interpolator& interpolator)
    : CPICurve(referenceDate, baseDate, baseCPI, lag, frequency, dayCounter, seasonality),
      QuantLib::InterpolatedCurve<Interpolator>(interpolator) {}

template <class T> QuantLib::Date InterpolatedCPIInflationCurve<T>::baseDate() const {
    if (hasExplicitBaseDate())
        return ZeroInflationTermStructure::baseDate();
    else
        return dates_.front();
}

template <class T> QuantLib::Date InterpolatedCPIInflationCurve<T>::maxDate() const {
    if (hasExplicitBaseDate())
        return dates_.back();
    else
        return QuantLib::inflationPeriod(dates_.back(), frequency()).second;
}

template <class T> inline QuantLib::Rate InterpolatedCPIInflationCurve<T>::forwardCPIImpl(QuantLib::Time t) const {
    return this->interpolation_(t, true);
}

template <class T> inline const std::vector<QuantLib::Time>& InterpolatedCPIInflationCurve<T>::times() const {
    return this->times_;
}

template <class T> inline const std::vector<QuantLib::Date>& InterpolatedCPIInflationCurve<T>::dates() const {
    return dates_;
}

template <class T> inline const std::vector<QuantLib::Rate>& InterpolatedCPIInflationCurve<T>::rates() const {
    return this->data_;
}

template <class T> inline const std::vector<QuantLib::Real>& InterpolatedCPIInflationCurve<T>::data() const {
    return this->data_;
}

template <class T>
inline std::vector<std::pair<QuantLib::Date, QuantLib::Rate>> InterpolatedCPIInflationCurve<T>::nodes() const {
    std::vector<std::pair<QuantLib::Date, QuantLib::Rate>> results(dates_.size());
    for (QuantLib::Size i = 0; i < dates_.size(); ++i)
        results[i] = std::make_pair(dates_[i], this->data_[i]);
    return results;
}

} // namespace QuantExt

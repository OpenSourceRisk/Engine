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

/*! \file qle/termstructures/optionletcurve.hpp
    \brief Interpolated one-dimensional curve of optionlet volatilities
*/

#ifndef quantext_optionlet_curve_hpp
#define quantext_optionlet_curve_hpp

#include <ql/math/comparison.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/termstructures/volatility/flatsmilesection.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>
#include <ql/utilities/dataformatters.hpp>

namespace QuantExt {

/*! OptionletVolatilityStructure based on interpolation of one-dimensional vector of optionlet volatilities

    The intended use case for this class is to represent the optionlet volatilities along a strike column of a cap
    floor volatility surface.

    \ingroup termstructures
*/
template <class Interpolator>
class InterpolatedOptionletCurve : public QuantLib::OptionletVolatilityStructure,
                                   protected QuantLib::InterpolatedCurve<Interpolator> {

public:
    /*! Constructor
        \param dates           The fixing dates of the underlying interest rate index
        \param volatilities    The optionlet volatility at each of the \p dates
        \param bdc             Business day convention used when getting an optionlet expiry date from an optionlet
                               expiry tenor
        \param dayCounter      The day counter used to convert dates to times
        \param calendar        The calendar used when getting an optionlet expiry date from an optionlet expiry tenor
                               and. Also used to advance from today to reference date if necessary.
        \param volatilityType  The volatility type of the provided \p volatilities
        \param displacement    The applicable shift size if the \p volatilityType is \c ShiftedLognormal
        \param flatFirstPeriod If the volatility between the first date and second date in \p dates is assumed constant
                               and equal to the second element of \p volatilities. This means that the first element of
                               \p volatilities is ignored.
        \param interpolator    The interpolation object used to interpolate between the provided \p dates
    */
    InterpolatedOptionletCurve(const std::vector<QuantLib::Date>& dates,
                               const std::vector<QuantLib::Real>& volatilities, QuantLib::BusinessDayConvention bdc,
                               const QuantLib::DayCounter& dayCounter,
                               const QuantLib::Calendar& calendar = QuantLib::Calendar(),
                               QuantLib::VolatilityType volatilityType = QuantLib::Normal,
                               QuantLib::Real displacement = 0.0, bool flatFirstPeriod = true,
                               const Interpolator& interpolator = Interpolator());

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const override;
    //@}

    //! \name VolatilityTermStructure interface
    //@{
    QuantLib::Rate minStrike() const override;
    QuantLib::Rate maxStrike() const override;
    //@}

    //! \name OptionletVolatilityStructure interface
    //@{
    QuantLib::VolatilityType volatilityType() const override;
    QuantLib::Real displacement() const override;
    //@}

    //! \name Other inspectors
    //@{
    const std::vector<QuantLib::Time>& times() const;
    const std::vector<QuantLib::Date>& dates() const;
    const std::vector<QuantLib::Real>& volatilities() const;
    const std::vector<QuantLib::Real>& data() const;
    std::vector<std::pair<QuantLib::Date, QuantLib::Real> > nodes() const;
    //@}

protected:
    InterpolatedOptionletCurve(QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dayCounter,
                               QuantLib::VolatilityType volatilityType = QuantLib::Normal,
                               QuantLib::Real displacement = 0.0, bool flatFirstPeriod = true,
                               const Interpolator& interpolator = Interpolator());

    InterpolatedOptionletCurve(const QuantLib::Date& referenceDate, const QuantLib::Calendar& calendar,
                               QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dayCounter,
                               QuantLib::VolatilityType volatilityType = QuantLib::Normal,
                               QuantLib::Real displacement = 0.0, bool flatFirstPeriod = true,
                               const Interpolator& interpolator = Interpolator());

    InterpolatedOptionletCurve(QuantLib::Natural settlementDays, const QuantLib::Calendar& calendar,
                               QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dayCounter,
                               QuantLib::VolatilityType volatilityType = QuantLib::Normal,
                               QuantLib::Real displacement = 0.0, bool flatFirstPeriod = true,
                               const Interpolator& interpolator = Interpolator());

    //! \name OptionletVolatilityStructure interface
    //@{
    /*! Gives a flat SmileSection at the requested \p optionTime. The flat value is obtained by interpolating the
        input volatilities at the given \p optionTime.
    */
    QuantLib::ext::shared_ptr<QuantLib::SmileSection> smileSectionImpl(QuantLib::Time optionTime) const override;

    /*! Gives the interpolated optionlet volatility at the requested \p optionTime. The \p strike is ignored.
     */
    QuantLib::Real volatilityImpl(QuantLib::Time optionTime, QuantLib::Rate strike) const override;
    //@}

    //! The fixing dates of the index underlying the optionlets
    mutable std::vector<QuantLib::Date> dates_;

private:
    //! The optionlet volatility type
    QuantLib::VolatilityType volatilityType_;

    //! If the volatility type is ShiftedLognormal, this holds the shift value
    QuantLib::Real displacement_;

    //! True if the volatility from the initial date to the first date is assumed flat
    bool flatFirstPeriod_;

    //! Initialise the dates and the interpolation object.
    void initialise();
};

/*! Term structure based on linear interpolation of optionlet volatilities
    \ingroup termstructures
*/
typedef InterpolatedOptionletCurve<QuantLib::Linear> OptionletCurve;

template <class Interpolator>
InterpolatedOptionletCurve<Interpolator>::InterpolatedOptionletCurve(
    const std::vector<QuantLib::Date>& dates, const std::vector<QuantLib::Real>& volatilities,
    QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dayCounter, const QuantLib::Calendar& calendar,
    QuantLib::VolatilityType volatilityType, QuantLib::Real displacement, bool flatFirstPeriod,
    const Interpolator& interpolator)
    : QuantLib::OptionletVolatilityStructure(dates.at(0), calendar, bdc, dayCounter),
      QuantLib::InterpolatedCurve<Interpolator>(std::vector<QuantLib::Time>(), volatilities, interpolator),
      dates_(dates), volatilityType_(volatilityType), displacement_(displacement), flatFirstPeriod_(flatFirstPeriod) {
    initialise();
}

template <class Interpolator>
InterpolatedOptionletCurve<Interpolator>::InterpolatedOptionletCurve(QuantLib::BusinessDayConvention bdc,
                                                                     const QuantLib::DayCounter& dayCounter,
                                                                     QuantLib::VolatilityType volatilityType,
                                                                     QuantLib::Real displacement, bool flatFirstPeriod,
                                                                     const Interpolator& interpolator)
    : QuantLib::OptionletVolatilityStructure(bdc, dayCounter), QuantLib::InterpolatedCurve<Interpolator>(interpolator),
      volatilityType_(volatilityType), displacement_(displacement), flatFirstPeriod_(flatFirstPeriod) {}

template <class Interpolator>
InterpolatedOptionletCurve<Interpolator>::InterpolatedOptionletCurve(
    const QuantLib::Date& referenceDate, const QuantLib::Calendar& calendar, QuantLib::BusinessDayConvention bdc,
    const QuantLib::DayCounter& dayCounter, QuantLib::VolatilityType volatilityType, QuantLib::Real displacement,
    bool flatFirstPeriod, const Interpolator& interpolator)
    : QuantLib::OptionletVolatilityStructure(referenceDate, calendar, bdc, dayCounter),
      QuantLib::InterpolatedCurve<Interpolator>(interpolator), volatilityType_(volatilityType),
      displacement_(displacement), flatFirstPeriod_(flatFirstPeriod) {}

template <class Interpolator>
InterpolatedOptionletCurve<Interpolator>::InterpolatedOptionletCurve(
    QuantLib::Natural settlementDays, const QuantLib::Calendar& calendar, QuantLib::BusinessDayConvention bdc,
    const QuantLib::DayCounter& dayCounter, QuantLib::VolatilityType volatilityType, QuantLib::Real displacement,
    bool flatFirstPeriod, const Interpolator& interpolator)
    : QuantLib::OptionletVolatilityStructure(settlementDays, calendar, bdc, dayCounter),
      QuantLib::InterpolatedCurve<Interpolator>(interpolator), volatilityType_(volatilityType),
      displacement_(displacement), flatFirstPeriod_(flatFirstPeriod) {}

template <class T> inline QuantLib::Date InterpolatedOptionletCurve<T>::maxDate() const {
    if (this->maxDate_ != Date())
        return this->maxDate_;
    return dates_.back();
}

template <class T> inline QuantLib::Rate InterpolatedOptionletCurve<T>::minStrike() const {
    if (volatilityType() == QuantLib::ShiftedLognormal) {
        return displacement_ > 0.0 ? -displacement_ : 0.0;
    } else {
        return QL_MIN_REAL;
    }
}

template <class T> inline QuantLib::Rate InterpolatedOptionletCurve<T>::maxStrike() const { return QL_MAX_REAL; }

template <class T> inline QuantLib::VolatilityType InterpolatedOptionletCurve<T>::volatilityType() const {
    return volatilityType_;
}

template <class T> inline QuantLib::Real InterpolatedOptionletCurve<T>::displacement() const { return displacement_; }

template <class T> inline const std::vector<QuantLib::Time>& InterpolatedOptionletCurve<T>::times() const {
    return this->times_;
}

template <class T> inline const std::vector<QuantLib::Date>& InterpolatedOptionletCurve<T>::dates() const {
    return dates_;
}

template <class T> inline const std::vector<QuantLib::Real>& InterpolatedOptionletCurve<T>::volatilities() const {
    if (flatFirstPeriod_)
        this->data_[0] = this->data_[1];
    return this->data_;
}

template <class T> inline const std::vector<QuantLib::Real>& InterpolatedOptionletCurve<T>::data() const {
    if (flatFirstPeriod_)
        this->data_[0] = this->data_[1];
    return this->data_;
}

template <class T>
inline std::vector<std::pair<QuantLib::Date, QuantLib::Real> > InterpolatedOptionletCurve<T>::nodes() const {
    std::vector<std::pair<QuantLib::Date, QuantLib::Real> > results(dates_.size());
    for (QuantLib::Size i = 0; i < dates_.size(); ++i)
        results[i] = std::make_pair(dates_[i], this->data_[i]);
    if (flatFirstPeriod_)
        results[0].second = this->data_[1];
    return results;
}

template <class T>
QuantLib::ext::shared_ptr<QuantLib::SmileSection>
InterpolatedOptionletCurve<T>::smileSectionImpl(QuantLib::Time optionTime) const {
    QuantLib::Volatility vol = volatility(optionTime, 0.0, true);
    return QuantLib::ext::make_shared<QuantLib::FlatSmileSection>(
        optionTime, vol, dayCounter(), QuantLib::Null<QuantLib::Rate>(), volatilityType_, displacement_);
}

template <class T>
QuantLib::Real InterpolatedOptionletCurve<T>::volatilityImpl(QuantLib::Time optionTime, QuantLib::Rate strike) const {
    if (flatFirstPeriod_ && optionTime < this->times_[1])
        return this->data_[1];
    else
        return this->interpolation_(optionTime, true);
}

template <class T> void InterpolatedOptionletCurve<T>::initialise() {

    QL_REQUIRE(dates_.size() >= T::requiredPoints, "Not enough input dates given for interpolation method");
    QL_REQUIRE(this->data_.size() == dates_.size(), "Number of dates does not equal the number of volatilities");

    this->times_.resize(dates_.size());
    this->times_[0] = 0.0;
    for (QuantLib::Size i = 1; i < dates_.size(); ++i) {
        QL_REQUIRE(dates_[i] > dates_[i - 1], "Dates must be increasing but "
                                                  << QuantLib::io::ordinal(i) << " date " << dates_[i]
                                                  << " is not after " << QuantLib::io::ordinal(i - 1) << " date "
                                                  << dates_[i - 1]);
        this->times_[i] = dayCounter().yearFraction(dates_[0], dates_[i]);
        QL_REQUIRE(!QuantLib::close(this->times_[i], this->times_[i - 1]),
                   "The " << QuantLib::io::ordinal(i) << " date " << dates_[i] << " and "
                          << QuantLib::io::ordinal(i - 1) << " date " << dates_[i - 1]
                          << " correspond to the same time, " << this->times_[i]
                          << ", under this curve's day count convention, " << dayCounter());
        QL_REQUIRE(this->data_[i] > 0.0,
                   "The " << QuantLib::io::ordinal(i) << " volatility, " << this->data_[i] << ", is not positive");
    }

    if (flatFirstPeriod_)
        this->data_[0] = this->data_[1];

    this->setupInterpolation();
}

} // namespace QuantExt

#endif

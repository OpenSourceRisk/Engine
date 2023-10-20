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

/*! \file qle/termstructures/capfloortermvolcurve.hpp
    \brief Cap floor at-the-money term volatility curve
    \ingroup termstructures
*/

#ifndef quantext_cap_floor_term_volatility_curve_hpp
#define quantext_cap_floor_term_volatility_curve_hpp

#include <ql/math/interpolation.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/termstructures/volatility/capfloor/capfloortermvolatilitystructure.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <vector>

namespace QuantExt {

/*! Cap floor term volatility curve.
    Abstract base class for one dimensional curve of cap floor volatilities.
*/
class CapFloorTermVolCurve : public QuantLib::CapFloorTermVolatilityStructure {
public:
    //! \name Constructors
    //@{
    CapFloorTermVolCurve(QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc = QuantLib::DayCounter())
        : QuantLib::CapFloorTermVolatilityStructure(bdc, dc) {}

    CapFloorTermVolCurve(const QuantLib::Date& referenceDate, const QuantLib::Calendar& cal,
                         QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc = QuantLib::DayCounter())
        : QuantLib::CapFloorTermVolatilityStructure(referenceDate, cal, bdc, dc) {}

    //! calculate the reference date based on the global evaluation date
    CapFloorTermVolCurve(QuantLib::Natural settlementDays, const QuantLib::Calendar& cal,
                         QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc = QuantLib::DayCounter())
        : QuantLib::CapFloorTermVolatilityStructure(settlementDays, cal, bdc, dc) {}
    //@}

    //! Return the tenors used in the CapFloorTermVolCurve
    virtual std::vector<QuantLib::Period> optionTenors() const = 0;
};

//! Interpolated cap floor term volatility curve
/*! Class that interpolates a vector of cap floor volatilities.

    Based on the class QuantLib::CapFloorTermVolCurve with changes:
    - allows for a user provided interpolation (main reason for copy)
    - does not derive from boost::noncopyable (is this needed?)
*/
template <class Interpolator>
class InterpolatedCapFloorTermVolCurve : public QuantLib::LazyObject,
                                         public CapFloorTermVolCurve,
                                         protected QuantLib::InterpolatedCurve<Interpolator> {

public:
    /*! Constructor with floating reference date
        \param settlementDays  Number of days from evaluation date to curve reference date.
        \param calendar        The calendar used to derive cap floor maturity dates from \p optionTenors.
                               Also used to advance from today to reference date if necessary.
        \param bdc             The business day convention used to derive cap floor maturity dates from \p optionTenors.
        \param optionTenors    The cap floor tenors. The first tenor must be positive.
        \param volatilities    The cap floor volatility quotes.
        \param dayCounter      The day counter used to convert from dates to times.
        \param flatFirstPeriod Set to \c true to use the first element of \p volatilities between time zero and the
                               first element of \p optionTenors. If this is \c false, the volatility at time zero is
                               set to zero and interpolation between time and the first element of \p optionTenors is
                               used.
        \param interpolator    An instance of the interpolator to use. Allows for specification of \p Interpolator
                               instances that use a constructor that takes arguments.
    */
    InterpolatedCapFloorTermVolCurve(QuantLib::Natural settlementDays, const QuantLib::Calendar& calendar,
                                     QuantLib::BusinessDayConvention bdc,
                                     const std::vector<QuantLib::Period>& optionTenors,
                                     const std::vector<QuantLib::Handle<QuantLib::Quote> >& volatilities,
                                     const QuantLib::DayCounter& dayCounter = QuantLib::Actual365Fixed(),
                                     bool flatFirstPeriod = true, const Interpolator& interpolator = Interpolator());

    /*! Constructor with fixed reference date
        \param settlementDate  The curve reference date.
        \param calendar        The calendar used to derive cap floor maturity dates from \p optionTenors.
                               Also used to advance from today to reference date if necessary.
        \param bdc             The business day convention used to derive cap floor maturity dates from \p optionTenors.
        \param optionTenors    The cap floor tenors. The first tenor must be positive.
        \param volatilities    The cap floor volatility quotes.
        \param dayCounter      The day counter used to convert from dates to times.
        \param flatFirstPeriod Set to \c true to use the first element of \p volatilities between time zero and the
                               first element of \p optionTenors. If this is \c false, the volatility at time zero is
                               set to zero and interpolation between time and the first element of \p optionTenors is
                               used.
        \param interpolator    An instance of the interpolator to use. Allows for specification of \p Interpolator
                               instances that use a constructor that takes arguments.
    */
    InterpolatedCapFloorTermVolCurve(const QuantLib::Date& settlementDate, const QuantLib::Calendar& calendar,
                                     QuantLib::BusinessDayConvention bdc,
                                     const std::vector<QuantLib::Period>& optionTenors,
                                     const std::vector<QuantLib::Handle<QuantLib::Quote> >& volatilities,
                                     const QuantLib::DayCounter& dayCounter = QuantLib::Actual365Fixed(),
                                     bool flatFirstPeriod = true, const Interpolator& interpolator = Interpolator());

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const override;
    //@}

    //! \name VolatilityTermStructure interface
    //@{
    QuantLib::Rate minStrike() const override;
    QuantLib::Rate maxStrike() const override;
    //@}

    //! \name LazyObject interface
    //@{
    void update() override;
    void performCalculations() const override;
    //@}

    //! \name CapFloorTermVolCurve interface
    //@{
    std::vector<QuantLib::Period> optionTenors() const override;
    //@}

    //! \name Inspectors
    //@{
    const std::vector<QuantLib::Date>& optionDates() const;
    const std::vector<QuantLib::Time>& optionTimes() const;
    //@}

protected:
    //! \name CapFloorTermVolatilityStructure interface
    //@{
    QuantLib::Volatility volatilityImpl(QuantLib::Time length, QuantLib::Rate) const override;
    //@}

private:
    //! Check the constructor arguments for consistency
    void checkInputs() const;

    //! Register this curve with the volatility quotes
    void registerWithMarketData();

    //! Number of underlying cap floor instruments
    QuantLib::Size nOptionTenors_;

    //! Underlying cap floor tenors
    std::vector<QuantLib::Period> optionTenors_;

    /*! Underlying cap floor maturity dates.
        Mutable since if the curve is \c moving, the dates need to be updated from \c const method.
    */
    mutable std::vector<QuantLib::Date> optionDates_;

    /*! Time to maturity of underlying cap floor instruments.
        Mutable since if the curve is \c moving, the times need to be updated from \c const method.
    */
    mutable std::vector<QuantLib::Time> optionTimes_;

    //! Cap floor term volatility quotes
    std::vector<QuantLib::Handle<QuantLib::Quote> > volatilities_;

    //! True for flat volatility from time zero to the first cap floor date
    bool flatFirstPeriod_;
};

template <class Interpolator>
InterpolatedCapFloorTermVolCurve<Interpolator>::InterpolatedCapFloorTermVolCurve(
    QuantLib::Natural settlementDays, const QuantLib::Calendar& calendar, QuantLib::BusinessDayConvention bdc,
    const std::vector<QuantLib::Period>& optionTenors,
    const std::vector<QuantLib::Handle<QuantLib::Quote> >& volatilities, const QuantLib::DayCounter& dayCounter,
    bool flatFirstPeriod, const Interpolator& interpolator)
    : CapFloorTermVolCurve(settlementDays, calendar, bdc, dayCounter), QuantLib::InterpolatedCurve<Interpolator>(
                                                                           optionTenors.size() + 1, interpolator),
      nOptionTenors_(optionTenors.size()), optionTenors_(optionTenors), optionDates_(nOptionTenors_),
      optionTimes_(nOptionTenors_), volatilities_(volatilities), flatFirstPeriod_(flatFirstPeriod) {

    checkInputs();
    registerWithMarketData();
}

template <class Interpolator>
InterpolatedCapFloorTermVolCurve<Interpolator>::InterpolatedCapFloorTermVolCurve(
    const QuantLib::Date& settlementDate, const QuantLib::Calendar& calendar, QuantLib::BusinessDayConvention bdc,
    const std::vector<QuantLib::Period>& optionTenors,
    const std::vector<QuantLib::Handle<QuantLib::Quote> >& volatilities, const QuantLib::DayCounter& dayCounter,
    bool flatFirstPeriod, const Interpolator& interpolator)
    : CapFloorTermVolCurve(settlementDate, calendar, bdc, dayCounter), QuantLib::InterpolatedCurve<Interpolator>(
                                                                           optionTenors.size() + 1, interpolator),
      nOptionTenors_(optionTenors.size()), optionTenors_(optionTenors), optionDates_(nOptionTenors_),
      optionTimes_(nOptionTenors_), volatilities_(volatilities), flatFirstPeriod_(flatFirstPeriod) {

    checkInputs();
    registerWithMarketData();
}

template <class Interpolator> QuantLib::Date InterpolatedCapFloorTermVolCurve<Interpolator>::maxDate() const {
    calculate();
    return optionDates_.back();
}

template <class Interpolator> QuantLib::Rate InterpolatedCapFloorTermVolCurve<Interpolator>::minStrike() const {
    return QL_MIN_REAL;
}

template <class Interpolator> QuantLib::Rate InterpolatedCapFloorTermVolCurve<Interpolator>::maxStrike() const {
    return QL_MAX_REAL;
}

template <class Interpolator> void InterpolatedCapFloorTermVolCurve<Interpolator>::update() {
    CapFloorTermVolatilityStructure::update();
    LazyObject::update();
}

template <class Interpolator> void InterpolatedCapFloorTermVolCurve<Interpolator>::performCalculations() const {

    // Populate the InterpolatedCurve members
    // We make the time zero volatility equal to zero here. However, if flatFirstPeriod is set to true, there is no
    // interpolation between time 0 and the first option date so this value of 0.0 is effectively ignored.
    this->times_[0] = 0.0;
    this->data_[0] = 0.0;
    for (Size i = 0; i < nOptionTenors_; ++i) {
        optionDates_[i] = optionDateFromTenor(optionTenors_[i]);
        optionTimes_[i] = timeFromReference(optionDates_[i]);
        this->times_[i + 1] = optionTimes_[i];
        this->data_[i + 1] = volatilities_[i]->value();
    }

    // Would rather call setupInterpolation() but it is not a const method
    this->interpolation_ =
        this->interpolator_.interpolate(this->times_.begin(), this->times_.end(), this->data_.begin());
}

template <class Interpolator>
std::vector<QuantLib::Period> InterpolatedCapFloorTermVolCurve<Interpolator>::optionTenors() const {
    calculate();
    return optionTenors_;
}

template <class Interpolator>
const std::vector<QuantLib::Date>& InterpolatedCapFloorTermVolCurve<Interpolator>::optionDates() const {
    calculate();
    return optionDates_;
}

template <class Interpolator>
const std::vector<QuantLib::Time>& InterpolatedCapFloorTermVolCurve<Interpolator>::optionTimes() const {
    calculate();
    return optionTimes_;
}

template <class Interpolator>
QuantLib::Volatility InterpolatedCapFloorTermVolCurve<Interpolator>::volatilityImpl(QuantLib::Time length,
                                                                                    QuantLib::Rate) const {

    calculate();

    if (flatFirstPeriod_ && length < this->times_[1])
        return this->data_[1];
    else
        return this->interpolation_(length, true);
}

template <class Interpolator> void InterpolatedCapFloorTermVolCurve<Interpolator>::checkInputs() const {
    QL_REQUIRE(!optionTenors_.empty(), "The option tenor vector cannot be empty");
    QL_REQUIRE(nOptionTenors_ == volatilities_.size(), "Mismatch between number of option tenors ("
                                                           << nOptionTenors_ << ") and number of volatilities ("
                                                           << volatilities_.size() << ")");
    QL_REQUIRE(optionTenors_[0] > 0 * Days, "First option tenor needs to be positive but is: " << optionTenors_[0]);
    for (QuantLib::Size i = 1; i < nOptionTenors_; ++i) {
        QL_REQUIRE(optionTenors_[i] > optionTenors_[i - 1],
                   "Non increasing option tenor: " << QuantLib::io::ordinal(i) << " is " << optionTenors_[i - 1]
                                                   << " and " << QuantLib::io::ordinal(i + 1) << " is "
                                                   << optionTenors_[i]);
    }
}

template <class Interpolator> void InterpolatedCapFloorTermVolCurve<Interpolator>::registerWithMarketData() {
    for (QuantLib::Size i = 0; i < volatilities_.size(); ++i)
        registerWith(volatilities_[i]);
}

} // namespace QuantExt

#endif

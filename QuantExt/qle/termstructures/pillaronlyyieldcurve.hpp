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

/*! \file qle/termstructures/pillaronlyyieldcurve.hpp
    \brief Yield curves that interpolate only on actual pillar dates, excluding synthetic t=0
    \ingroup termstructures
*/

#pragma once

#include <ql/math/comparison.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/termstructures/yield/forwardstructure.hpp>
#include <ql/termstructures/yield/zeroyieldstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

using namespace QuantLib;

//! Discount curve interpolating on pillar dates only, excluding synthetic t=0
/*! This curve interpolates discount factors on actual market pillar dates only,
    excluding the synthetic time-zero point. This is useful when the first market
    pillar does not coincide with the as-of date and you want to avoid including
    a synthetic discount factor point in the interpolation.

    Key features:
    - At t=0: Returns DF(0) = 1.0 by definition
    - Left extrapolation (0 < t < t₁): Flat forward rate from first pillar ensuring continuity
    - Interpolation (t₁ ≤ t ≤ tₙ): Uses provided interpolator on pillar discount factors
    - Right extrapolation (t > tₙ): Flat instantaneous forward rate from last pillar

    \ingroup termstructures
*/
template <class Interpolator>
class InterpolatedPillarOnlyDiscountCurve : public YieldTermStructure, protected InterpolatedCurve<Interpolator> {
public:
    //! \name Constructors
    //@{
    InterpolatedPillarOnlyDiscountCurve(const Date& referenceDate, const std::vector<Date>& dates,
                                        const std::vector<DiscountFactor>& discounts, const DayCounter& dayCounter,
                                        const Interpolator& interpolator = Interpolator(),
                                        const Extrapolation extrapolation = Extrapolation::ContinuousForward);
    //@}

    //! \name YieldTermStructure interface
    //@{
    Date maxDate() const override;
    //@}

protected:
    //! \name YieldTermStructure implementation
    //@{
    DiscountFactor discountImpl(Time t) const override;
    //@}

private:
    Extrapolation extrapolation_;
    std::vector<Date> dates_;
};

//! Zero rate curve interpolating on pillar dates only, excluding synthetic t=0
/*! This curve interpolates continuously compounded zero rates on actual market
    pillar dates only, excluding the synthetic time-zero point.

    Key features:
    - At t=0: Returns DF(0) = 1.0 by definition
    - Left extrapolation (0 < t < t₁): Flat zero rate from first pillar
    - Interpolation (t₁ ≤ t ≤ tₙ): Uses provided interpolator on pillar zero rates
    - Right extrapolation (t > tₙ): Flat zero rate from last pillar
    - Discount factors computed as: DF(t) = exp(-z(t) * t)

    \ingroup termstructures
*/
template <class Interpolator>
class InterpolatedPillarOnlyZeroCurve : public ZeroYieldStructure, protected InterpolatedCurve<Interpolator> {
public:
    //! \name Constructors
    //@{
    InterpolatedPillarOnlyZeroCurve(const Date& referenceDate, const std::vector<Date>& dates,
                                    const std::vector<Rate>& zeroRates, const DayCounter& dayCounter,
                                    const Interpolator& interpolator = Interpolator(),
                                    const Extrapolation extrapolation = Extrapolation::ContinuousForward);
    //@}

    //! \name YieldTermStructure interface
    //@{
    Date maxDate() const override;
    //@}

protected:
    //! \name ZeroYieldStructure implementation
    //@{
    DiscountFactor zeroYieldImpl(Time t) const override;
    //@}

private:
    Extrapolation extrapolation_;
    std::vector<Date> dates_;
};

//! Forward rate curve interpolating on pillar dates only, excluding synthetic t=0
/*! This curve interpolates instantaneous forward rates on actual market pillar
    dates only, excluding the synthetic time-zero point.

    Key features:
    - At t=0: Returns DF(0) = 1.0 by definition
    - Left extrapolation (0 < t < t₁): Flat forward rate from first pillar
    - Interpolation (t₁ ≤ t ≤ tₙ): Uses provided interpolator on pillar forward rates
    - Right extrapolation (t > tₙ): Flat forward rate from last pillar
    - Discount factors computed by numerical integration: DF(t) = exp(-∫₀ᵗ f(s)ds)

    \ingroup termstructures
*/
template <class Interpolator>
class InterpolatedPillarOnlyForwardCurve : public ForwardRateStructure, protected InterpolatedCurve<Interpolator> {
public:
    //! \name Constructors
    //@{
    InterpolatedPillarOnlyForwardCurve(const Date& referenceDate, const std::vector<Date>& dates,
                                       const std::vector<Rate>& forwardRates, const DayCounter& dayCounter,
                                       const Interpolator& interpolator = Interpolator(),
                                       const Extrapolation extrapolation = Extrapolation::ContinuousForward);
    //@}

    //! \name YieldTermStructure interface
    //@{
    Date maxDate() const override;
    //@}

protected:
    //! \name ForwardRateStructure implementation
    //@{
        Rate forwardImpl(Time t) const override;
        Rate zeroYieldImpl(Time t) const override;
    //@}

private:
    Extrapolation extrapolation_;
    std::vector<Date> dates_;
};

// Template implementations

template <class Interpolator>
InterpolatedPillarOnlyDiscountCurve<Interpolator>::InterpolatedPillarOnlyDiscountCurve(
    const Date& referenceDate, const std::vector<Date>& dates, const std::vector<DiscountFactor>& discounts,
    const DayCounter& dayCounter, const Interpolator& interpolator, const Extrapolation extrapolation)
    : YieldTermStructure(referenceDate, Calendar(), dayCounter), InterpolatedCurve<Interpolator>(interpolator),
      extrapolation_(extrapolation), dates_(dates) {

    QL_REQUIRE(!dates.empty(), "InterpolatedPillarOnlyDiscountCurve: dates cannot be empty");
    QL_REQUIRE(dates.size() == discounts.size(),
               "InterpolatedPillarOnlyDiscountCurve: dates and discounts must have the same size");

    // Store discount factors
    this->data_.resize(discounts.size());
    for (Size i = 0; i < discounts.size(); ++i) {
        this->data_[i] = discounts[i];
    }

    // Setup interpolation on pillar times
    this->setupTimes(dates_, referenceDate, dayCounter);
    this->setupInterpolation();
    this->interpolation_.update();
}

template <class Interpolator> Date InterpolatedPillarOnlyDiscountCurve<Interpolator>::maxDate() const {
    return dates_.back();
}

template <class Interpolator>
DiscountFactor InterpolatedPillarOnlyDiscountCurve<Interpolator>::discountImpl(Time t) const {
    // At reference date (t=0), return 1.0 by definition
    if (t <= 0.0 || QuantLib::close_enough(t, 0.0)) {
        return 1.0;
    }

    // For times between t=0 and first pillar, extrapolate backwards using flat forward from first pillar
    // This ensures continuity: as t approaches 0 from the right, DF approaches 1.0
    if (t < this->times_.front()) {
        Time t1 = this->times_.front();
        DiscountFactor df1 = this->data_.front();
        // Flat forward extrapolation: df(t) = df(t1) * exp(r * (t1 - t)) where r = -ln(df1)/t1
        Rate r = -std::log(df1) / t1;
        return df1 * std::exp(r * (t1 - t));
    }

    // Interpolate on pillars for times within the curve range
    if (t <= this->times_.back()) {
        return this->interpolation_(t, true);
    }

    // Flat forward extrapolation beyond the last pillar
    Time tMax = this->times_.back();
    DiscountFactor dMax = this->data_.back();
    if (extrapolation_ == YieldTermStructure::Extrapolation::ContinuousForward) {
        Rate instFwdMax = -this->interpolation_.derivative(tMax, true) / dMax;
        return dMax * std::exp(-instFwdMax * (t - tMax));
    } else if (extrapolation_ == YieldTermStructure::Extrapolation::DiscreteForward) {
        Time tMax_m = this->timeFromReference(dates_.back() - 1);
        DiscountFactor dMax_m = this->interpolation_(tMax_m, true);
        return dMax * std::pow(dMax / dMax_m, (t - tMax) / (tMax - tMax_m));
    } else {
        QL_FAIL("extrapolation method not handled.");
    }
}

template <class Interpolator>
InterpolatedPillarOnlyZeroCurve<Interpolator>::InterpolatedPillarOnlyZeroCurve(
    const Date& referenceDate, const std::vector<Date>& dates, const std::vector<Rate>& zeroRates,
    const DayCounter& dayCounter, const Interpolator& interpolator, const Extrapolation extrapolation)
    : ZeroYieldStructure(referenceDate, Calendar(), dayCounter), InterpolatedCurve<Interpolator>(interpolator),
      extrapolation_(extrapolation), dates_(dates) {

    QL_REQUIRE(!dates.empty(), "InterpolatedPillarOnlyZeroCurve: dates cannot be empty");
    QL_REQUIRE(dates.size() == zeroRates.size(),
               "InterpolatedPillarOnlyZeroCurve: dates and zeroRates must have the same size");

    // Store zero rates
    this->data_.resize(zeroRates.size());
    for (Size i = 0; i < zeroRates.size(); ++i) {
        this->data_[i] = zeroRates[i];
    }

    // Setup interpolation on pillar times
    this->setupTimes(dates_, referenceDate, dayCounter);
    this->setupInterpolation();
    this->interpolation_.update();
}

template <class Interpolator> Date InterpolatedPillarOnlyZeroCurve<Interpolator>::maxDate() const {
    return dates_.back();
}

template <class Interpolator> DiscountFactor InterpolatedPillarOnlyZeroCurve<Interpolator>::zeroYieldImpl(Time t) const {
    if (t < this->times_.front()) {
        // For times between t=0 and first pillar, flat extrapolation using first pillar rate
        return this->data_.front();
    } else if (t <= this->times_.back()) {
        // Interpolate on pillars for times within the curve range
        return this->interpolation_(t, true);
    } else {
        // flat fwd extrapolation
        Time tMax = this->times_.back();
        Rate zMax = this->data_.back();
        if (extrapolation_ == YieldTermStructure::Extrapolation::ContinuousForward) {
            Rate instFwdMax = zMax + tMax * this->interpolation_.derivative(tMax, true);
            return (zMax * tMax + instFwdMax * (t - tMax)) / t;
        } else if (extrapolation_ == YieldTermStructure::Extrapolation::DiscreteForward) {
            Time tMax_m = this->timeFromReference(dates_.back() - 1);
            Rate dz = this->interpolation_(tMax) - this->interpolation_(tMax_m, true);
            return (zMax * tMax + dz * (t - tMax) / (tMax - tMax_m)) / t;
        } else {
            QL_FAIL("extrapolation method not handled.");
        }
    }
}

template <class Interpolator>
InterpolatedPillarOnlyForwardCurve<Interpolator>::InterpolatedPillarOnlyForwardCurve(
    const Date& referenceDate, const std::vector<Date>& dates, const std::vector<Rate>& forwardRates,
    const DayCounter& dayCounter, const Interpolator& interpolator, const Extrapolation extrapolation)
    : ForwardRateStructure(referenceDate, Calendar(), dayCounter), InterpolatedCurve<Interpolator>(interpolator),
      extrapolation_(extrapolation), dates_(dates) {

    QL_REQUIRE(!dates.empty(), "InterpolatedPillarOnlyForwardCurve: dates cannot be empty");
    QL_REQUIRE(dates.size() == forwardRates.size(),
               "InterpolatedPillarOnlyForwardCurve: dates and forwardRates must have the same size");

    // Store forward rates
    this->data_.resize(forwardRates.size());
    for (Size i = 0; i < forwardRates.size(); ++i) {
        this->data_[i] = forwardRates[i];
    }

    // Setup interpolation on pillar times
    this->setupTimes(dates_, referenceDate, dayCounter);
    this->setupInterpolation();
    this->interpolation_.update();
}

template <class Interpolator> Date InterpolatedPillarOnlyForwardCurve<Interpolator>::maxDate() const {
    return dates_.back();
}

template <class Interpolator>
DiscountFactor InterpolatedPillarOnlyForwardCurve<Interpolator>::forwardImpl(Time t) const {
    if (t < this->times_.front()) {
        Rate f = this->data_.front();
        return f;
    }

    if (t <= this->times_.back()) {
        return this->interpolation_(t, true);
    } else {
        // flat fwd extrapolation
        if (extrapolation_ == YieldTermStructure::Extrapolation::ContinuousForward) {
            return this->data_.back();
        } else if (extrapolation_ == YieldTermStructure::Extrapolation::DiscreteForward) {
            Time tMax = this->times_.back();
            Time tMax_m = this->timeFromReference(dates_.back() - 1);
            Real iMax = this->interpolation_.primitive(tMax, true);
            Real iMax_m = this->interpolation_.primitive(tMax_m, true);
            return (iMax - iMax_m) / (tMax - tMax_m);
        } else {
            QL_FAIL("extrapolation method not handled.");
        }
    }
}

template <class Interpolator>
DiscountFactor InterpolatedPillarOnlyForwardCurve<Interpolator>::zeroYieldImpl(Time t) const {
    // For times between t=0 and first pillar, use flat forward from first pillar
    if (t < this->times_.front()) {
        Rate f = this->data_.front();
        return f;
    }

    // For times within the curve range, integrate forward rates
    Real integral;
    if (t <= this->times_.back()) {
        integral = this->interpolation_.primitive(t, true);
    } else {
        // flat fwd extrapolation
        if (extrapolation_ == YieldTermStructure::Extrapolation::ContinuousForward) {
            integral = this->interpolation_.primitive(this->times_.back(), true) +
                       this->data_.back() * (t - this->times_.back());
        } else if (extrapolation_ == YieldTermStructure::Extrapolation::DiscreteForward) {
            Time tMax = this->times_.back();
            Time tMax_m = this->timeFromReference(dates_.back() - 1);
            Real iMax = this->interpolation_.primitive(tMax, true);
            Real iMax_m = this->interpolation_.primitive(tMax_m, true);
            integral = iMax + (iMax - iMax_m) * (t - tMax) / (tMax - tMax_m);
        } else {
            QL_FAIL("extrapolation method not handled.");
        }
    }
    return integral / t;
}

} // namespace QuantExt

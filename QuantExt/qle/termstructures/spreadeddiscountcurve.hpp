/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file spreadeddiscountcurve.hpp
    \brief spreaded discount term structure
    \ingroup termstructures
*/

#pragma once

#include <ql/math/interpolation.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {
using namespace QuantLib;

/*! Curve taking a reference curve and discount factor quotes, that are used to overlay the reference
  curve with a spread. The quotes are interpolated loglinearly. The spread curve is given in terms of
  times relative to the reference date, which means that the spread will float with a changing reference
  date in the reference curve. */
class SpreadedDiscountCurve : public YieldTermStructure, public LazyObject {
public:
    enum class Interpolation { logLinear, linearZero };
    enum class Extrapolation { flatFwd, flatZero };
    //! times should be consistent with reference ts day counter
    SpreadedDiscountCurve(const Handle<YieldTermStructure>& referenceCurve, const std::vector<Time>& times,
                          const std::vector<Handle<Quote>>& quotes,
                          const Interpolation interpolation = Interpolation::logLinear,
                          const Extrapolation extrapolation = Extrapolation::flatFwd);

    Date maxDate() const override;
    void update() override;
    const Date& referenceDate() const override;

    Calendar calendar() const override;
    Natural settlementDays() const override;

protected:
    void performCalculations() const override;
    DiscountFactor discountImpl(Time t) const override;

private:
    Handle<YieldTermStructure> referenceCurve_;
    std::vector<Time> times_;
    std::vector<Handle<Quote>> quotes_;
    Interpolation interpolation_;
    Extrapolation extrapolation_;
    mutable std::vector<Real> data_;
    QuantLib::ext::shared_ptr<QuantLib::Interpolation> dataInterpolation_;
};

} // namespace QuantExt

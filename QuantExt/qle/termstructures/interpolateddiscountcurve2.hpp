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

/*! \file interpolateddiscountcurve2.hpp
    \brief interpolated discount term structure
    \ingroup termstructures
*/

#pragma once

#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {
using namespace QuantLib;

//! InterpolatedDiscountCurve2 as in QuantLib, but with floating discount quotes and floating reference date
/*! InterpolatedDiscountCurve2 as in QuantLib, but with
    floating discount quotes and floating reference date,
    reference date is always the global evaluation date,
    i.e. settlement days are zero and calendar is NullCalendar()

        \ingroup termstructures
*/
class InterpolatedDiscountCurve2 : public YieldTermStructure, public LazyObject {
public:
    enum class Interpolation { logLinear, linearZero };
    enum class Extrapolation { flatFwd, flatZero };
    //! \name Constructors
    //@{
    //! times based constructor, note that times should be consistent with day counter dc passed
    InterpolatedDiscountCurve2(const std::vector<Time>& times, const std::vector<Handle<Quote>>& quotes,
                               const DayCounter& dc, const Interpolation interpolation = Interpolation::logLinear,
                               const Extrapolation extrapolation = Extrapolation::flatFwd);
    //! date based constructor
    InterpolatedDiscountCurve2(const std::vector<Date>& dates, const std::vector<Handle<Quote>>& quotes,
                               const DayCounter& dc, const Interpolation interpolation = Interpolation::logLinear,
                               const Extrapolation extrapolation = Extrapolation::flatFwd);
    //@}

    void makeThisCurveSpreaded(const Handle<YieldTermStructure>& base);

    Date maxDate() const override { return Date::maxDate(); }
    void update() override;
    const Date& referenceDate() const override;

    Calendar calendar() const override { return NullCalendar(); }
    Natural settlementDays() const override { return 0; }

protected:
    void performCalculations() const override;

    DiscountFactor discountImpl(Time t) const override;

private:
    std::vector<Time> times_;
    std::vector<Handle<Quote>> quotes_;
    Interpolation interpolation_;
    Extrapolation extrapolation_;
    mutable std::vector<Real> data_;
    mutable Date today_;
    QuantLib::ext::shared_ptr<QuantLib::Interpolation> dataInterpolation_;
    Handle<YieldTermStructure> base_;
    std::vector<Real> baseOffset_;
};

} // namespace QuantExt

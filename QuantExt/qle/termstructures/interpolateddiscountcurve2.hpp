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

#ifndef quantext_interpolated_discount_curve_2_hpp
#define quantext_interpolated_discount_curve_2_hpp

#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {
//! InterpolatedDiscountCurve2 as in QuantLib, but with floating discount quotes and floating reference date
/*! InterpolatedDiscountCurve2 as in QuantLib, but with
    floating discount quotes and floating reference date,
    reference date is always the global evaluation date,
    i.e. settlement days are zero and calendar is NullCalendar()

        \ingroup termstructures
*/
class InterpolatedDiscountCurve2 : public YieldTermStructure, public LazyObject {
public:
    //! \name Constructors
    //@{
    //! times based constructor, note that times should be consistent with day counter dc passed
    InterpolatedDiscountCurve2(const std::vector<Time>& times, const std::vector<Handle<Quote> >& quotes,
                               const DayCounter& dc)
        : YieldTermStructure(dc), times_(times), quotes_(quotes), data_(times_.size(), 1.0),
          today_(Settings::instance().evaluationDate()) {
        for (Size i = 0; i < quotes.size(); ++i) {
            QL_REQUIRE(times_.size() > 1, "at least two times required");
            QL_REQUIRE(times_.size() == quotes.size(), "size of time and quote vectors do not match");
            QL_REQUIRE(times_[0] == 0.0, "First time must be 0, got " << times_[0]);
            QL_REQUIRE(!quotes[i].empty(), "quote at index " << i << " is empty");
            registerWith(quotes_[i]);
        }
        interpolation_ = boost::make_shared<LogLinearInterpolation>(times_.begin(), times_.end(), data_.begin());
        registerWith(Settings::instance().evaluationDate());
    }
    //! date based constructor
    InterpolatedDiscountCurve2(const std::vector<Date>& dates, const std::vector<Handle<Quote> >& quotes,
                               const DayCounter& dc)
        : YieldTermStructure(dc), times_(dates.size(), 0.0), quotes_(quotes), data_(dates.size(), 1.0),
          today_(Settings::instance().evaluationDate()) {
        for (Size i = 0; i < dates.size(); ++i)
            times_[i] = dc.yearFraction(today_, dates[i]);
        for (Size i = 0; i < quotes.size(); ++i) {
            QL_REQUIRE(times_.size() > 1, "at least two times required");
            QL_REQUIRE(times_.size() == quotes.size(), "size of time and quote vectors do not match");
            QL_REQUIRE(times_[0] == 0.0, "First time must be 0, got " << times_[0]);
            QL_REQUIRE(!quotes[i].empty(), "quote at index " << i << " is empty");
            registerWith(quotes_[i]);
        }
        interpolation_ = boost::make_shared<LogLinearInterpolation>(times_.begin(), times_.end(), data_.begin());
        registerWith(Settings::instance().evaluationDate());
    }
    //@}

    Date maxDate() const { return Date::maxDate(); }
    void update() {
        LazyObject::update();
        TermStructure::update();
    }
    const Date& referenceDate() const {
        calculate();
        return today_;
    }

    Calendar calendar() const { return NullCalendar(); }
    Natural settlementDays() const { return 0; }

protected:
    void performCalculations() const {
        today_ = Settings::instance().evaluationDate();
        for (Size i = 0; i < times_.size(); ++i) {
            data_[i] = quotes_[i]->value();
            QL_REQUIRE(data_[i] > 0, "InterpolatedDiscountCurve2: invalid value " << data_[i] << " at index " << i);
        }
        interpolation_->update();
    }

    DiscountFactor discountImpl(Time t) const {
        calculate();
        if (t <= this->times_.back())
            return (*interpolation_)(t, true);
        // flat fwd extrapolation
        Time tMax = this->times_.back();
        DiscountFactor dMax = this->data_.back();
        Rate instFwdMax = -(*interpolation_).derivative(tMax) / dMax;
        return dMax * std::exp(-instFwdMax * (t - tMax));
    }

private:
    std::vector<Time> times_;
    std::vector<Handle<Quote> > quotes_;
    mutable std::vector<Real> data_;
    mutable Date today_;
    boost::shared_ptr<Interpolation> interpolation_;
};

} // namespace QuantExt

#endif
